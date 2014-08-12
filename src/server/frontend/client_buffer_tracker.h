/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

 #ifndef MIR_FRONTEND_CLIENT_BUFFER_TRACKER_H_
 #define MIR_FRONTEND_CLIENT_BUFFER_TRACKER_H_

#include "surface_tracker.h"
#include <unordered_map>
#include <stdint.h>
#include <list>

namespace mir
{

namespace graphics
{
class BufferID;
}

namespace frontend
{

/// Responsible for tracking what buffers the client library knows about for a surface.

/// \sa mir::client::ClientBufferDepository for the client-side of this tracking
/// \note Changes to the tracking algorithm of mir::client::ClientBufferDepository will need to be mirrored here
class ClientBufferTracker
{
public:
    ClientBufferTracker(unsigned int client_cache_size);

    ClientBufferTracker(ClientBufferTracker const&) = delete;
    ClientBufferTracker& operator=(ClientBufferTracker const&) = delete;

    /// Add a BufferID to the list of buffers known by the client.
    ///
    /// Typically this should be done just prior to or just after sending the buffer information
    void add(graphics::BufferID const& id);
    bool client_has(graphics::BufferID const& id) const;
private:

    std::list<graphics::BufferID> ids;
    unsigned int const cache_size;
};

class SessionSurfaceTracker : public SurfaceTracker
{
public:
    SessionSurfaceTracker(size_t client_cache_size);
    SessionSurfaceTracker(SessionSurfaceTracker const&) = delete;
    SessionSurfaceTracker& operator=(SessionSurfaceTracker const&) = delete;

    bool track_buffer(SurfaceId, graphics::Buffer*) override;
    void remove_surface(SurfaceId) override;
    graphics::Buffer* last_buffer(SurfaceId) const override;
private:
    size_t const client_cache_size;

    std::unordered_map<SurfaceId,graphics::Buffer*> client_buffer_resource;
    std::unordered_map<SurfaceId, std::shared_ptr<ClientBufferTracker>> client_buffer_tracker;
};

}
}

#endif // MIR_FRONTEND_CLIENT_BUFFER_TRACKER_H_
