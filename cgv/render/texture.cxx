#include "texture.h"
#include "frame_buffer.h"

#include <cgv/media/image/image_reader.h>
#include <cgv/media/image/image_writer.h>
#include <cgv/utils/tokenizer.h>
#include <cgv/utils/file.h>

using namespace cgv::utils;
using namespace cgv::utils::file;
using namespace cgv::media::image;

namespace cgv {
	namespace render {

/** construct from description string and most commonly used
    texture parameters. The description can define a component
	 format as described in \c cgv::data::component_format::set_component_format()
	 or a data format as described in \c cgv::data::data_format::set_data_format(). */
texture::texture(const std::string& description, 
		  TextureFilter _mag_filter, 
	     TextureFilter _min_filter, 
		  TextureWrap   _wrap_s,
		  TextureWrap   _wrap_t,
		  TextureWrap   _wrap_r) : data::data_format(description)
{
	mag_filter = _mag_filter; 
	min_filter = _min_filter; 
	wrap_s = _wrap_s;	
	wrap_t = _wrap_t;	
	wrap_r = _wrap_r;
	state_out_of_date = true;
	have_mipmaps = false;
	tex_unit = -1;
}

/** destruct texture, the destructor can be called without context 
    if the destruct method has been called or no creation has 
	 taken place */
texture::~texture()
{
	if (handle != 0) {
		if (ctx_ptr == 0)
			std::cerr << "texture not destructed" << std::endl;
		else {
			if (!ctx_ptr->is_current())
				ctx_ptr->make_current();
			destruct(*ctx_ptr);
		}
	}
}

/// change the data format and clear internal format
bool texture::set_data_format(const std::string& description)
{
	bool res = data_format::set_data_format(description);
	internal_format = 0;
	state_out_of_date = true;
	return res;
}

/// change component format and clear internal format
void texture::set_component_format(const component_format& cf)
{
	data_format::set_component_format(cf);
	internal_format = 0;
	state_out_of_date = true;
}

/// change component format and clear internal format
void texture::set_component_format(const std::string& description)
{
	component_format::set_component_format(description);
	internal_format = 0;
	state_out_of_date = true;
}

/// set the texture wrap behaviour in s direction
void texture::set_wrap_s(TextureWrap _wrap_s)
{
	wrap_s = _wrap_s;
	state_out_of_date = true;
}
/// set the texture wrap behaviour in t direction
void texture::set_wrap_t(TextureWrap _wrap_t)
{
	wrap_t = _wrap_t;
	state_out_of_date = true;
}
/// set the texture wrap behaviour in r direction
void texture::set_wrap_r(TextureWrap _wrap_r)
{
	wrap_r = _wrap_r;
	state_out_of_date = true;
}
/// return the texture wrap behaviour in s direction
TextureWrap texture::get_wrap_s() const
{
	return wrap_s;
}
/// return the texture wrap behaviour in t direction
TextureWrap texture::get_wrap_t() const
{
	return wrap_t;
}
/// return the texture wrap behaviour in r direction
TextureWrap texture::get_wrap_r() const
{
	return wrap_r;
}
/// set the border color
void texture::set_border_color(float r, float g, float b, float a)
{
	border_color[0] = r;
	border_color[1] = g;
	border_color[2] = b;
	border_color[3] = a;
	state_out_of_date = true;
}
/** set the minification filters, if minification is set to TF_ANISOTROP, 
    the second floating point parameter specifies the degree of anisotropy */
void texture::set_min_filter(TextureFilter _min_filter, float _anisotropy)
{
	min_filter = _min_filter;
	anisotropy = _anisotropy;
	state_out_of_date = true;
}

/// return the minification filter
TextureFilter texture::get_min_filter() const
{
	return min_filter;
}
/// return the currently set anisotropy
float texture::get_anisotropy() const
{
	return anisotropy;
}
/// set the magnification filter
void texture::set_mag_filter(TextureFilter _mag_filter)
{
	mag_filter = _mag_filter;
	state_out_of_date = true;
}
/// return the magnification filter
TextureFilter texture::get_mag_filter() const
{
	return mag_filter;
}
/// set priority with which texture is kept in GPU memory
void texture::set_priority(float _priority)
{
	priority = _priority;
	state_out_of_date = true;
}
/// return the priority with which texture is kept in GPU memory
float texture::get_priority() const
{
	return priority;
}
//@}

/**@name methods that can be called only with a context */
//@{
/// find the format that matches the one specified in the component format best
void texture::find_best_format(context& ctx, const std::vector<data_view>* palettes)
{
	cgv::data::component_format cf = ctx.texture_find_best_format(*this, *this, palettes);
	void* tmp = internal_format;
	set_component_format(cf);
	internal_format = tmp;
}

void texture::ensure_state(context& ctx) const
{
	if (state_out_of_date) {
		ctx.texture_set_state(*this);
		state_out_of_date = false;
	}
}

/** create the texture of dimension and resolution specified in 
    the data format base class */
bool texture::create(context& ctx, TextureType _tt, unsigned width, unsigned height, unsigned depth)
{
	if (is_created())
		destruct(ctx);
	if (_tt != TT_UNDEF)
		tt = _tt;
	if (width != -1)
		set_width(width);
	if (height != -1)
		set_height(height);
	if (depth != -1)
		set_depth(depth);
	if (!internal_format)
		find_best_format(ctx);
	return complete_create(ctx, ctx.texture_create(*this, *this));
}

bool is_power_of_two(unsigned int i)
{
	do {
		if (i == 1)
			return true;
		if ((i & 1) != 0)
			return false;
		i /= 2;
	} 
	while (true);
	return false;
}

unsigned int power_of_two_ub(unsigned int i)
{
	unsigned int res = 2;
	while (res < i)
		res *= 2;
	return res;
}

bool texture::create_from_image(cgv::data::data_format& df, cgv::data::data_view& dv, context& ctx, 
								const std::string& file_name, unsigned char* clear_color_ptr, int level, int cube_side)
{
	bool ensure_power_of_two = clear_color_ptr != 0;
	std::string fn = file_name;
	if (df.empty()) {
		if (fn.empty()) {
			cgv::render::render_component::last_error = "attempt to create texture from empty file name";
			return false;
		}
	}
	std::vector<data_format> palette_formats;
	std::vector<data_view> palettes;
	if (!fn.empty()) {
		image_reader ir(df, &palette_formats);
		if (!ir.read_image(fn, dv)) {
			cgv::render::render_component::last_error = "error reading image file ";
			cgv::render::render_component::last_error += fn;
			cgv::render::render_component::last_error += ": ";
			cgv::render::render_component::last_error += ir.get_last_error();
			return false;
		}
		for (unsigned i=0; i<palette_formats.size(); ++i) {
			palettes.push_back(data_view());
			if (!ir.read_palette(i, palettes.back())) {
				cgv::render::render_component::last_error = "error reading palette";
				return false;
			}
		}
	}
	if (cube_side < 1)
		destruct(ctx);
	unsigned int w = df.get_width();
	unsigned int h = df.get_height();
	if (ensure_power_of_two && (
		 !is_power_of_two(w) ||
		 !is_power_of_two(h))) {
		unsigned int W = power_of_two_ub(df.get_width());
		unsigned int H = power_of_two_ub(df.get_height());
		float ext[2] = { (float)w/W, (float)h/H };
		data_format df1(df);
		df1.set_width(W);
		df1.set_height(H);
		data_view dv1(&df1);
		unsigned int es = df1.get_entry_size();
		unsigned char* dest_ptr = dv1.get_ptr<unsigned char>();
		const unsigned char* src_ptr = dv.get_ptr<unsigned char>();
		unsigned char* dest_ptr_end = dest_ptr+es*W*H;
		std::fill(dest_ptr, dest_ptr_end, clear_color_ptr?*clear_color_ptr:0);
		for (unsigned int y=0; y<h; ++y, dest_ptr += es*W, src_ptr += es*w)
			memcpy(dest_ptr, src_ptr, es*w);
		return create(ctx, dv1, level, cube_side, &palettes);
	}
	else
		return create(ctx, dv, level, cube_side, &palettes);
}


/** create the texture from an image file*/
bool texture::create_from_image(context& ctx, 
	const std::string& file_name, int* image_width_ptr, int* image_height_ptr,
	unsigned char* clear_color_ptr, int level, int cube_side)
{
	data_format df;
	data_view dv;
	if (!create_from_image(df,dv,ctx,file_name,clear_color_ptr,level,cube_side))
		return false;
	if (image_width_ptr)
		*image_width_ptr = df.get_width();
	unsigned int h = df.get_height();
	if (image_height_ptr)
		*image_height_ptr = df.get_height();
	return true;
}

bool texture::deduce_file_names(const std::string& file_names, std::vector<std::string>& deduced_names)
{
	if (std::find(file_names.begin(), file_names.end(), '{') != file_names.end()) {
		std::vector<token> toks;
		deduced_names.resize(6);
		tokenizer(file_names).set_ws("").set_sep("{,}").bite_all(toks);
		int selection = -1;
		for (unsigned i=0; i<toks.size(); ++i) {
			if (toks[i] == "{") {
				if (selection != -1) {
					std::cerr << "warning: nested {} not allowed in cubemap file names specification " << file_names << std::endl;
					return false;
				}
				selection = 0;
			}
			else if (toks[i] == "}") {
				if (selection == -1) {
					std::cerr << "warning: } without opening { in cubemap file names specification " << file_names << std::endl;
					return false;
				}
				if (selection != 5) {
					std::cerr << "warning: no 6 file names specified for creating cubemap from images " << file_names << std::endl;
					return false;
				}
				selection = -1;
			}
			else if (toks[i] == ",") {
				if (selection == -1) {
					std::cerr << "warning: , arising outside {} in cubemap file names specification " << file_names << std::endl;
					return false;
				}
				++selection;
			}
			else {
				if (selection == -1) {
					for (unsigned j=0; j<6; ++j)
						deduced_names[j] += to_string(toks[i]);
				}
				else {
					if (selection == 6) {
						std::cerr << "warning: more than 6 files names specified for cubemap creation " << file_names << std::endl;
						return false;
					}
					deduced_names[selection] += to_string(toks[i]);
				}
			}
		}
	}
	else {
		std::string path_prefix = get_path(file_names);
		if (!path_prefix.empty())
			path_prefix += '/';
		void* handle = find_first(file_names);
		while (handle != 0) {
			deduced_names.push_back(path_prefix+find_name(handle));
			handle = find_next(handle);
		}
		if (deduced_names.size() != 6) {
			std::cerr << "warning: not exactly 6 files names specified for cubemap creation " << file_names << std::endl;
			return false;
		}
	}
	return true;
}

bool texture::create_from_images(context& ctx, const std::string& file_names, int level)
{
	std::vector<std::string> deduced_names;
	if (!deduce_file_names(file_names, deduced_names))
		return false;
	bool success = true;
	for (unsigned i=0; success && i<6; ++i)
		if (!create_from_image(ctx, deduced_names[i],0,0,0,level,i)) {
			success = false;
			std::cerr << "could not create cubemap side " << i << " from image " << deduced_names[i] << std::endl;
		}
	if (!success)
		destruct(ctx);
	return success;
}

/// write the content of the texture to a file. This method needs support for frame buffer objects.
bool texture::write_to_file(context& ctx, const std::string& file_name, unsigned int z_or_cube_size) const
{
	std::string& last_error = static_cast<const cgv::render::texture_base*>(this)->last_error;
	if (!is_created()) {
		last_error = "texture must be created for write_to_file";
		return false;
	}
	frame_buffer fb;
	if (!fb.create(ctx,get_width(), get_height())) {
		last_error = "could not create frame buffer object for write_to_file";
		return false;
	}
	if (z_or_cube_size != -1) {
		if (!fb.attach(ctx,*this,z_or_cube_size,0,0)) {
			last_error = "could not attach texture for write_to_file";
			return false;
		}
	}
	else {
		if (!fb.attach(ctx,*this)) {
			last_error = "could not attach texture for write_to_file";
			return false;
		}
	}
	bool is_depth = get_standard_component_format() == CF_D;
	data_format df("uint8[R,G,B]");
	render_buffer rb("[D]");
	if (is_depth) {
		rb = render_buffer("[R,G,B]");
		df = data_format("uint8[D]");
	}
	rb.create(ctx,get_width(), get_height());
	if (!fb.attach(ctx, rb)) {
		last_error = "could not attach render buffer for write_to_file";
		return false;
	}
	if (!fb.is_complete(ctx)) {
		last_error = "frame buffer object not complete for write_to_file due to\n";
		last_error += fb.last_error;
		return false;
	}
	fb.enable(ctx);
		df.set_width(get_width());
		df.set_height(get_height());
		data_view dv(&df);
		ctx.read_frame_buffer(dv);
	fb.disable(ctx);
	image_writer w(file_name);
	if (!w.write_image(dv)) {
		last_error = "could not write image file ";
		last_error += file_name;
		return false;
	}
	return true;
}


/** generate mipmaps automatically, only supported if 
    framebuffer objects are supported by the GPU */
bool texture::generate_mipmaps(context& ctx)
{
	return ctx.texture_generate_mipmaps(*this, get_nr_dimensions());
}

bool texture::complete_create(context& ctx, bool created)
{
	state_out_of_date = true;
	ctx_ptr = &ctx;
	if (created)
		ensure_state(ctx);
	return created;
}


/** create texture from the currently set read buffer, where
    x, y, width and height define the to be used rectangle of 
	 the read buffer. The dimension and resolution of the texture
	 format are updated automatically. If level is not specified 
	 or set to -1 mipmaps are generated. */
bool texture::create_from_buffer(context& ctx, int x, int y, int width, int height, int level)
{
	if (is_created() && level < 1)
		return replace_from_buffer(ctx, 0, 0, x, y, width, height, level);

	if (level <= 0) {
		destruct(ctx);
		set_nr_dimensions(2);
		set_width(width);
		set_height(height);
	}
	if (level == -1 && !internal_format)
		find_best_format(ctx);
	return complete_create(ctx, ctx.texture_create_from_buffer(*this, *this, x, y, level));
}

/** create texture from data view. Use dimension and resolution
    of data view but the component format of the texture.
    If level is not specified or set to -1 mipmaps are generated. */
bool texture::create(context& ctx, const cgv::data::const_data_view& data, int level, int cube_side, const std::vector<data_view>* palettes)
{
	const data_format& f = *data.get_format();
	TextureType tt = (TextureType)f.get_nr_dimensions();
	if (tt == TT_2D && cube_side != -1)
		tt = TT_CUBEMAP;
	if ( (tt != TT_CUBEMAP && is_created()) ) {
		bool replace_allowed = tt == this->tt;
			for (unsigned i=0; replace_allowed && i<get_nr_dimensions(); ++i)
				if (get_resolution(i) != f.get_resolution(i))
					replace_allowed = false;
		if (replace_allowed && level < 1) {
			switch (tt) {
			case TT_1D : return replace(ctx, 0, data, level, palettes);
			case TT_2D : return replace(ctx, 0, 0, data, level, palettes);
			case TT_3D : return replace(ctx, 0, 0, 0, data, level, palettes);
			}
			return false;
		}
		destruct(ctx);
	}
	if (level < 1) {
		set_nr_dimensions(data.get_format()->get_nr_dimensions());
		for (unsigned int i=0; i<get_nr_dimensions(); ++i)
			set_resolution(i, data.get_format()->get_resolution(i));
		static_cast<component_format&>(*this) = *data.get_format();
		if (level == -1 && !internal_format)
			find_best_format(ctx, palettes);
	}
	return complete_create(ctx, ctx.texture_create(*this, *this, data, level, cube_side, palettes));
}

/** replace a block within a 1d texture with the given data. 
    If level is not specified, level 0 is set and if a mipmap 
	 has been created before, coarser levels are updated also. */
bool texture::replace(context& ctx, int x, const cgv::data::const_data_view& data, int level, const std::vector<data_view>* palettes)
{
	if (!is_created()) {
		render_component::last_error = "attempt to replace in a not created 1d texture";
		return false;
	}
	if (data.get_format()->get_nr_dimensions() > 1) {
		render_component::last_error = "cannot replace block in 1d texture with 2d or 3d data block";
		return false;
	}
	return ctx.texture_replace(*this,x,-1,-1,data,level, palettes);
}

/** replace a block within a 2d texture with the given data. 
    If level is not specified, level 0 is set and if a mipmap 
	 has been created before, coarser levels are updated also. */
bool texture::replace(context& ctx, int x, int y, const cgv::data::const_data_view& data, int level, const std::vector<data_view>* palettes)
{
	if (!is_created()) {
		render_component::last_error = "attempt to replace in a not created 1d texture";
		return false;
	}
	if (data.get_format()->get_nr_dimensions() > 2) {
		render_component::last_error = "cannot replace block in 2d texture with 3d data block";
		return false;
	}
	return ctx.texture_replace(*this,x,y,-1,data,level, palettes);
}

/** replace a block within a 3d texture or a side of a cube map
    with the given data. 
    If level is not specified, level 0 is set and if a mipmap 
	 has been created before, coarser levels are updated also. */
bool texture::replace(context& ctx, int x, int y, int z_or_cube_side, const cgv::data::const_data_view& data, int level, const std::vector<data_view>* palettes)
{
	if (!is_created()) {
		render_component::last_error = "attempt to replace in a not created 1d texture";
		return false;
	}
	bool res = ctx.texture_replace(*this,x,y,z_or_cube_side,data,level, palettes);
	if (res && level == -1 && !have_mipmaps)
		have_mipmaps = true;
	return res;
}

/// replace a block within a 2d texture from the current read buffer.
bool texture::replace_from_buffer(context& ctx, int x, int y, int x_buffer, 
	int y_buffer, int width, int height, int level)
{
	if (!is_created()) {
		render_component::last_error = "attempt to replace in a not created 1d texture";
		return false;
	}
	if (get_nr_dimensions() != 2) {
		render_component::last_error = "attempt to replace a 2d block in a not 2d texture";
		return false;
	}
	return ctx.texture_replace_from_buffer(*this,x,y,-1,x_buffer,y_buffer,width,height,level);
}

/// replace a block within a 2d texture from the current read buffer.
bool texture::replace_from_buffer(context& ctx, int x, int y, int z_or_cube_side, int x_buffer, 
		int y_buffer, int width, int height, int level)
{
	if (!is_created()) {
		render_component::last_error = "attempt to replace in a not created texture";
		return false;
	}
	if (tt != TT_3D && tt != TT_CUBEMAP) {
		render_component::last_error = "replacing a 2d block in a slice / cube side of a not 3d texture / cubemap";
		return false;
	}
	return ctx.texture_replace_from_buffer(*this,x,y,z_or_cube_side,x_buffer,y_buffer,width,height,level);
}

/// replace within a slice of a volume or a side of a cube map from the given image
bool texture::replace_from_image(context& ctx, const std::string& file_name, int x, int y, int z_or_cube_side, int level)
{
	data_format df;
	data_view dv;
	return replace_from_image(df,dv,ctx,file_name,x,y,z_or_cube_side,level);
}

/** same as previous method but use the passed data format and data view to
    store the content of the image. */
bool texture::replace_from_image(cgv::data::data_format& df, cgv::data::data_view& dv, context& ctx, 
								 const std::string& file_name, int x, int y, int z_or_cube_side, 
								 int level)
{
	std::string fn = file_name;
	if (fn.empty()) {
		return false;
	}
	std::vector<data_format> palette_formats;
	image_reader ir(df, &palette_formats);
	if (!ir.read_image(fn, dv))
		return false;
	std::vector<data_view> palettes;
	for (unsigned i=0; i<palette_formats.size(); ++i) {
		palettes.push_back(data_view());
		if (!ir.read_palette(i, palettes.back()))
			return false;
	}
	destruct(ctx);
	unsigned int w = df.get_width();
	unsigned int h = df.get_height();
	replace(ctx, x, y, z_or_cube_side, dv, level, &palettes);
	return true;
}


/// destruct the texture and free texture memory and handle
bool texture::destruct(context& ctx)
{
	state_out_of_date = true;
	return ctx.texture_destruct(*this);
}

//@}

/**@name methods that change the current gpu context */
//@{
/** enable this texture in the given texture unit, -1 corresponds to 
    the current unit. */
bool texture::enable(context& ctx, int _tex_unit)
{
	if (!handle) {
		render_component::last_error = "attempt to enable texture that is not created";
		return false;
	}
	ensure_state(ctx);
	tex_unit = _tex_unit;
	return ctx.texture_enable(*this, tex_unit, get_nr_dimensions());
}
/// disable texture and restore state before last enable call
bool texture::disable(context& ctx)
{
	return ctx.texture_disable(*this, tex_unit, get_nr_dimensions());
}

/// check whether mipmaps have been created
bool texture::mipmaps_created() const
{
	return have_mipmaps;
}
/// check whether textue is enabled
bool texture::is_enabled() const
{
	return user_data != 0;
}
/// return the currently used texture unit and -1 for current
int texture::get_tex_unit() const
{
	return tex_unit;
}


//@}
	}
}