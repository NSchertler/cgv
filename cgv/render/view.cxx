#include <cgv/render/view.h>

using namespace cgv::math;

namespace cgv {
	namespace render {

/// construct a parallel view with focus in the world origin looking in negative z-direction and the y-direction as up-direction with an extent of +-1
view::view() 
	: focus(0,0,0), view_up_dir(0,1,0), view_dir(0,0,-1), y_view_angle(0), y_extent_at_focus(2)
{
}
///
view::pnt_type view::get_focus() const
{
	return focus;
}
///
view::vec_type view::get_view_up_dir() const
{
	return view_up_dir;
}
///
view::vec_type view::get_view_dir() const
{
	return view_dir;
}
///
double view::get_y_view_angle() const
{
	return y_view_angle;
}
///
double view::get_y_extent_at_focus() const
{
	return y_extent_at_focus;
}
/// 
void view::set_focus(const pnt_type& foc) 
{
	focus = foc;
}
void view::set_focus(double x, double y, double z) { set_focus(pnt_type(x,y,z)); }
void view::set_focus(const double* foc) { set_focus(pnt_type(foc)); }

///
void view::set_view_up_dir(const vec_type& vud)
{
	view_up_dir = vud;
}
void view::set_view_up_dir(double x, double y, double z) { set_view_up_dir(vec_type(x,y,z)); }
void view::set_view_up_dir(const double* vud) { set_view_up_dir(vec_type(vud)); }
///
void view::set_view_dir(const vec_type& vd)
{
	view_dir = vd;
}
void view::set_view_dir(double x, double y, double z) { set_view_dir(vec_type(x,y,z)); }
void view::set_view_dir(const double* vd) { set_view_dir(vec_type(vd)); }
///
void view::set_y_extent_at_focus(double ext)
{
	y_extent_at_focus = ext;
}
///
void view::set_y_view_angle(double angle)
{
	y_view_angle = angle;
}

/// return whether the y view angle is zero
bool view::is_parallel() const
{
	return y_view_angle == 0;
}
/// return the eye point, which is only valid if the view is not parallel
const view::pnt_type view::get_eye() const
{
	return focus-(y_extent_at_focus/tan(.008726646260*y_view_angle))*view_dir;
}
/// set the view dir and y-extent at focus such that get_eye() returns the passed point. This does not work in case that the eye point is identical to the focus point.
void view::set_eye_keep_view_angle(const pnt_type& eye)
{
	view_dir = focus-eye;
	y_extent_at_focus = tan(.008726646260*y_view_angle)*view_dir.length();
	view_dir.normalize();
}

/// set the view dir and angle such the get_eye() returns the passed point. This does not work in case that the eye point is identical to the focus point.
void view::set_eye_keep_extent(const pnt_type& eye)
{
	view_dir = focus-eye;
	y_view_angle = atan(y_extent_at_focus/view_dir.length())*114.5915590;
	view_dir.normalize();
}

/// set the view according to the standard view lookat definition from eye, focus and view up direction.
void view::view_look_at_keep_extent(const pnt_type& e, const pnt_type& foc, const vec_type& vud)
{
	set_focus(foc);
	set_view_up_dir(vud);
	set_eye_keep_extent(e);
}

/// set the view according to the standard view lookat definition from eye, focus and view up direction.
void view::view_look_at_keep_view_angle(const pnt_type& e, const pnt_type& foc, const vec_type& vud)
{
	set_focus(foc);
	set_view_up_dir(vud);
	set_eye_keep_view_angle(e);
}

	}
}