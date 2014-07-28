/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Robert Carr <robert.carr@canonical.com>
 */

#include "android_buffer_writer.h"
#include "buffer.h"

#include "mir/graphics/android/android_native_buffer.h"

#include <hardware/hardware.h>

#include <boost/throw_exception.hpp>
#include <stdexcept>

#include <string.h>

namespace mg = mir::graphics;
namespace mga = mg::android;

mga::AndroidBufferWriter::AndroidBufferWriter()
{
    int err;

    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (hw_module_t const **)(&hw_module));
    if (err < 0)
        BOOST_THROW_EXCEPTION(std::runtime_error("Could not open hardware module"));
}

void mga::AndroidBufferWriter::write(std::shared_ptr<mg::Buffer> const& buffer, void const* data, size_t size)
{
    // TODO: Size validation etc...
    auto mga_buffer = std::dynamic_pointer_cast<mga::Buffer>(buffer);
    if (!mga_buffer)
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid buffer (did not originate from platform graphics driver)"));
    
    auto handle = buffer->native_buffer_handle();
    
    auto buffer_size = buffer->size();
    if (buffer->stride().as_uint32_t() * buffer_size.height.as_uint32_t() != size)
        BOOST_THROW_EXCEPTION(std::logic_error("Size of pixels is not equal to size of buffer"));
    
    char* vaddr;
    int usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
    int width = buffer->size().width.as_uint32_t();
    int height = buffer->size().height.as_uint32_t();
    int top = 0;
    int left = 0;
    if ( hw_module->lock(hw_module, handle->handle(),
                              usage, top, left, width, height, (void**) &vaddr) )
        BOOST_THROW_EXCEPTION(std::runtime_error("error securing buffer for client cpu use"));

    memcpy(vaddr, data, size);
    
    hw_module->unlock(hw_module, handle->handle());
}
