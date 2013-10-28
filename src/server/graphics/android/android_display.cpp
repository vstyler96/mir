/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/platform.h"
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/display_report.h"
#include "mir/graphics/display_buffer.h"
#include "mir/graphics/gl_context.h"
#include "mir/graphics/egl_resources.h"
#include "android_display.h"
#include "android_display_buffer_factory.h"
#include "display_device.h"
#include "mir/geometry/rectangle.h"

#include <boost/throw_exception.hpp>

#include <system/window.h>
#include <algorithm>
#include <stdexcept>

namespace mga=mir::graphics::android;
namespace mg=mir::graphics;
namespace geom=mir::geometry;

namespace
{

static EGLint const dummy_pbuffer_attribs[] =
{
    EGL_WIDTH, 1,
    EGL_HEIGHT, 1,
    EGL_NONE
};

static EGLint const default_egl_context_attr[] =
{
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

#if 0
static EGLint const default_egl_config_attr [] =
{
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

static EGLConfig select_egl_config(EGLDisplay egl_display, ANativeWindow const& native_window)
{
    int num_potential_configs, android_native_id;
    EGLint num_match_configs;

    eglGetConfigs(egl_display, NULL, 0, &num_potential_configs);
    std::vector<EGLConfig> config_slots(num_potential_configs);

    /* upon return, this will fill config_slots[0:num_match_configs] with the matching */
    eglChooseConfig(egl_display, default_egl_config_attr,
                    config_slots.data(), num_potential_configs, &num_match_configs);
    config_slots.resize(num_match_configs);

    /* why check manually for EGL_NATIVE_VISUAL_ID instead of using eglChooseConfig? the egl
     * specification does not list EGL_NATIVE_VISUAL_ID as something it will check for in
     * eglChooseConfig */
    native_window.query(&native_window, NATIVE_WINDOW_FORMAT, &android_native_id);
    auto const pegl_config = std::find_if(begin(config_slots), end(config_slots),
        [&](EGLConfig& current) -> bool
        {
            int visual_id;
            eglGetConfigAttrib(egl_display, current, EGL_NATIVE_VISUAL_ID, &visual_id);
            return (visual_id == android_native_id);
        });

    if (pegl_config == end(config_slots))
        BOOST_THROW_EXCEPTION(std::runtime_error("could not select EGL config for use with framebuffer"));

    return *pegl_config;
}

EGLDisplay create_and_initialize_display()
{
    EGLint major, minor;

    auto egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY)
        BOOST_THROW_EXCEPTION(std::runtime_error("eglGetDisplay failed\n"));

    if (eglInitialize(egl_display, &major, &minor) == EGL_FALSE)
        BOOST_THROW_EXCEPTION(std::runtime_error("eglInitialize failure\n"));

    if ((major != 1) || (minor != 4))
        BOOST_THROW_EXCEPTION(std::runtime_error("must have EGL 1.4\n"));
    return egl_display;
}
#endif

class AndroidGLContext : public mg::GLContext
{
public:
    AndroidGLContext(EGLDisplay egl_display, EGLConfig egl_config, EGLContext egl_context_shared)
        : egl_display{egl_display},
          egl_context{egl_display,
                      eglCreateContext(egl_display, egl_config, egl_context_shared,
                                       default_egl_context_attr)},
          egl_surface{egl_display,
                      eglCreatePbufferSurface(egl_display, egl_config,
                                              dummy_pbuffer_attribs)}
    {
    }

    ~AndroidGLContext() noexcept
    {
        if (eglGetCurrentContext() == egl_context)
            release_current();
    }

    void make_current()
    {
        if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) == EGL_FALSE)
        {
            BOOST_THROW_EXCEPTION(std::runtime_error("could not activate dummy surface with eglMakeCurrent\n"));
        }
    }

    void release_current()
    {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

private:
    EGLDisplay const egl_display;
    mg::EGLContextStore const egl_context;
    mg::EGLSurfaceStore const egl_surface;
};

}

mga::AndroidDisplay::AndroidDisplay(std::shared_ptr<mga::AndroidDisplayBufferFactory> const& db_factory,
                                    std::shared_ptr<DisplayReport> const& display_report)
    : db_factory{db_factory},
      egl_surface_dummy{db_factory->egl_display(),
                        eglCreatePbufferSurface(db_factory->egl_display(),
                                                db_factory->egl_config(),
                                                dummy_pbuffer_attribs)},
      display_device(db_factory->create_display_device()),
      display_buffer{db_factory->create_display_buffer(display_device)},
      current_configuration{display_buffer->view_area().size}
{
    display_report->report_successful_setup_of_native_resources();

    /* Make the shared context current */
    if (eglMakeCurrent(db_factory->egl_display(),
                       egl_surface_dummy,
                       egl_surface_dummy,
                       db_factory->shared_egl_context()) == EGL_FALSE)
        BOOST_THROW_EXCEPTION(std::runtime_error("could not activate dummy surface with eglMakeCurrent\n"));

    display_report->report_successful_egl_make_current_on_construction();
    display_report->report_successful_display_construction();
    display_report->report_egl_configuration(db_factory->egl_display(), db_factory->egl_config());
}

mga::AndroidDisplay::~AndroidDisplay()
{
    eglMakeCurrent(db_factory->egl_display(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
//    eglTerminate(egl_display);
}

void mga::AndroidDisplay::for_each_display_buffer(std::function<void(mg::DisplayBuffer&)> const& f)
{
    f(*display_buffer);
}

std::shared_ptr<mg::DisplayConfiguration> mga::AndroidDisplay::configuration()
{
    return std::make_shared<mga::AndroidDisplayConfiguration>(current_configuration);
}

void mga::AndroidDisplay::configure(mg::DisplayConfiguration const& configuration)
{
    configuration.for_each_output([&](mg::DisplayConfigurationOutput const& output)
    {
        // TODO: Properly support multiple outputs
        display_device->mode(output.power_mode);
    });
    current_configuration = dynamic_cast<mga::AndroidDisplayConfiguration const&>(configuration);
}

void mga::AndroidDisplay::register_configuration_change_handler(
    EventHandlerRegister&,
    DisplayConfigurationChangeHandler const&)
{
}

void mga::AndroidDisplay::register_pause_resume_handlers(
    EventHandlerRegister& /*handlers*/,
    DisplayPauseHandler const& /*pause_handler*/,
    DisplayResumeHandler const& /*resume_handler*/)
{
}

void mga::AndroidDisplay::pause()
{
}

void mga::AndroidDisplay::resume()
{
}

auto mga::AndroidDisplay::the_cursor() -> std::weak_ptr<Cursor>
{
    return std::weak_ptr<Cursor>();
}

std::unique_ptr<mg::GLContext> mga::AndroidDisplay::create_gl_context()
{
    return std::unique_ptr<AndroidGLContext>{
        new AndroidGLContext{db_factory->egl_display(), db_factory->egl_config(), db_factory->shared_egl_context()}};
}
