/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_TEST_MOCK_ID_GENERATOR_H_
#define MIR_TEST_MOCK_ID_GENERATOR_H_

#include "mir/compositor/buffer_id.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct MockIDGenerator: public compositor::BufferIDUniqueGenerator
{
    MockIDGenerator()
    {
        using namespace testing;
        id = compositor::BufferID{34};
        ON_CALL(*this, generate_unique_id())
            .WillByDefault(Return(id));
    }

    MOCK_METHOD0(generate_unique_id, compositor::BufferID());

    compositor::BufferID id;
};

}
}
}

#endif /* MIR_TEST_MOCK_ID_GENERATOR_H_ */
