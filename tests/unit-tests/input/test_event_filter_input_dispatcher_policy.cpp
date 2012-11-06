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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "mir/input/event_filter.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mir_test/empty_deleter.h"

#include <androidfw/Input.h>

namespace mi = mir::input;

namespace
{
struct MockEventFilter : public mi::EventFilter
{
    MOCK_METHOD1(filter_event, bool(android::InputEvent*));
};
}

TEST(EventFilterDispatcherPolicy, offers_events_to_filter)
{
    using namespace ::testing;
    auto ev = new android::KeyEvent();
    MockEventFilter filter;
    mi::EventFilterDispatcherPolicy policy(std::shared_ptr<MockEventFilter>(&filter, mir::EmptyDeleter()));
    
    // Filter will pass the event on twice
    EXPECT_CALL(*filter, filter_event(_)).Times(1).WillOnce(Return(false));
    
    // Android uses alternate notation...the policy returns true if the event was NOT handled (e.g. the EventFilter
    // returns false)
    EXPECT_EQ(policy.filterInputEvent(ev, 0), true);    
}

