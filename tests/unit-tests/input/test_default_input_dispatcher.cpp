/*
 * Copyright © 2015 Canonical Ltd.
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

#define MIR_INCLUDE_DEPRECATED_EVENT_HEADER

#include "src/server/input/default_input_dispatcher.h"

#include "mir/events/event_builders.h"

#include "mir_test/event_matchers.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/stub_input_scene.h"
#include "mir_test_doubles/mock_input_surface.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <mutex>

namespace mi = mir::input;
namespace mev = mir::events;
namespace mt = mir::test;
namespace mtd = mt::doubles;

// TODO: Could eliminate many local event variables by passingdirectly to ::dispatch

namespace
{

struct StubInputScene : public mtd::StubInputScene
{
    std::shared_ptr<mtd::MockInputSurface> add_surface()
    {
        std::lock_guard<std::mutex> lg(surface_guard);
        
        auto surface = std::make_shared<mtd::MockInputSurface>();
        surfaces.push_back(surface);
        return surface;
    }
    
    void for_each(std::function<void(std::shared_ptr<mi::Surface> const&)> const& exec) override
    {
        std::lock_guard<std::mutex> lg(surface_guard);
        for (auto const& surface : surfaces)
            exec(surface);
    }
    
    std::mutex surface_guard;
    std::vector<std::shared_ptr<mi::Surface>> surfaces;
};

struct DefaultInputDispatcher : public testing::Test
{
    DefaultInputDispatcher()
        : dispatcher(mt::fake_shared(scene))
    {
    }

    void TearDown() override { dispatcher.stop(); }

    mi::DefaultInputDispatcher dispatcher;
    StubInputScene scene;
};

mir::EventUPtr a_key_event(MirKeyboardAction action = mir_keyboard_action_down)
{
    // TODO
    return mev::make_event(0, 0, action, 0, 0, mir_input_event_modifier_alt);
}
   
}

TEST_F(DefaultInputDispatcher, key_event_delivered_to_focused_surface)
{
    using namespace ::testing;
    
    auto surface = scene.add_surface();

    auto event = a_key_event();

    EXPECT_CALL(*surface, consume(mt::MirKeyEventMatches(*event))).Times(1);

    dispatcher.start();

    dispatcher.set_focus(surface);
    EXPECT_TRUE(dispatcher.dispatch(*event));
}

TEST_F(DefaultInputDispatcher, key_event_dropped_if_no_surface_focused)
{
    using namespace ::testing;
    
    auto surface = scene.add_surface();
    auto event = a_key_event();
    
    EXPECT_CALL(*surface, consume(_)).Times(0);

    dispatcher.start();
    EXPECT_FALSE(dispatcher.dispatch(*event));
}

TEST_F(DefaultInputDispatcher, inconsistent_key_events_dropped)
{
    using namespace ::testing;

    auto surface = scene.add_surface();
    auto event = a_key_event(mir_keyboard_action_up);

    EXPECT_CALL(*surface, consume(mt::MirKeyEventMatches(*event))).Times(0);

    dispatcher.start();

    dispatcher.set_focus(surface);
    
    EXPECT_FALSE(dispatcher.dispatch(*event));
}

TEST_F(DefaultInputDispatcher, key_state_is_consistent_per_client)
{
    using namespace ::testing;

    auto surface_1 = scene.add_surface();
    auto surface_2 = scene.add_surface();

    auto down_event = a_key_event();
    auto up_event= a_key_event(mir_keyboard_action_up);

    EXPECT_CALL(*surface_1, consume(mt::MirKeyEventMatches(*down_event))).Times(1);
    EXPECT_CALL(*surface_2, consume(_)).Times(0);

    dispatcher.start();

    dispatcher.set_focus(surface_1);
    EXPECT_TRUE(dispatcher.dispatch(*down_event));
    dispatcher.set_focus(surface_2);
    EXPECT_TRUE(dispatcher.dispatch(*up_event));
}
// TODO: Audit key state tests but this seems like a good start...

// TODO: Impl pointer eventbuidler
// TODO: Impl surfacecoords
// TODO: Impl hiding surface
// TODO: Implement scene remove surface and observer
// TODO: Test that pointer motion is relative
TEST_F(DefaultInputDispatcher, pointer_motion_delivered_to_client_under_pointer)
{
    using namespace ::testing;

    auto surface = scene.add_surface({{0, 0}, {5, 5}});

    auto ev_1 = a_pointer_event({1, 0});
    auto ev_2 = a_pointer_event({5, 0});

    InSequence seq;
    // The cursor begins at 0, 0
    EXPECT_CALL(*surface, consume(mt::PointerEnterEvent())).Times(1);
    EXPECT_CALL(*surface, consume(mt::PointerEventWithPosition(1, 0))).Times(1);
    // TODO: Test coordinate on PointerExit and PointerEnter event
    EXPECT_CALL(*surface, consume(mt::PointerLeaveEvent())).Times(1);

    dispatcher.start();

    EXPECT_TRUE(dispatcher.dispatch(*ev_1));
    EXPECT_TRUE(dispatcher.dispatch(*ev_2));
}

TEST_F(DefaultInputDispatcher, pointer_delivered_only_to_top_surface);
{
    using namespace ::testing;

    auto surface = scene.add_surface({{0, 0}, {5, 5}});
    auto top_surface = scene.add_surface({{0, 0}, {5, 5}});

    auto ev_1 = a_pointer_event({1, 0});

    InSequence seq;
    // The cursor begins at 0, 0
    EXPECT_CALL(*top_surface, consume(mt::PointerEnterEvent())).Times(1);
    EXPECT_CALL(*top_surface, consume(mt::PointerEventWithPosition(1, 0))).Times(1);
    EXPECT_CALL(*surface, consume(mt::PointerEnterEvent())).Times(1);
    EXPECT_CALL(*surface, consume(mt::PointerEventWithPosition(1, 0))).Times(1);
    
    dispatcher.start();

    EXPECT_TRUE(dispatcher.dispatch(*ev_1));
    scene.remove_surface(top_surface);
    EXPECT_TRUE(dispatcher.dispatch(*ev_1));
}

TEST_F(DefaultInputDispatcher, pointer_may_move_between_adjacent_surfaces)
{
    using namespace ::testing;
    
    auto surface = scene.add_surface({{0, 0}, {5, 5}});
    auto another_surface = scene.add_surface({{5, 5}, {5, 5}});

    auto ev_1 = a_pointer_event({6, 6});
    auto ev_2 = a_pointer_event({1, 1});

    InSequence seq;
    EXPECT_CALL(*surface, consume(mt::PointerEnterEvent())).Times(1);
    // TODO: Position on leave event
    EXPECT_CALL(*surface, consume(mt::PointerLeaveEvent())).Times(1);
    EXPECT_CALL(*another_surface, consume(mt::PointerEnterEvent())).Times(1);
    EXPECT_CALL(*another_surface, consume(mt::PointerEventWithPosition(1, 0))).Times(1);
    
    dispatcher.start();

    EXPECT_TRUE(dispatcher.dispatch(*ev_1));
    EXPECT_TRUE(dispatcher.dispatch(*ev_2));
}

// We test that a client will receive pointer events following a button down
// until the pointer comes up.
// TODO: What happens when a client dissapears during a gesture?
TEST_F(DefaultInputDispatcher, gestures_persist_over_button_down)
{
    using namespace ::testing;
    
    auto surface = scene.add_surface({{0, 0}, {5, 5}});
    auto another_surface = scene.add_surface({{5, 5}, {5, 5}});

    auto ev_1 = a_pointer_down_event();
    auto ev_2 = a_pointer_event({6, 6});
    auto ev_3 = a_pointer_up_event();

    InSequence seq;
    EXPECT_CALL(*surface, consume(mt::PointerEnterEvent())).Times(1);
    EXPECT_CALL(*surface, consume(mt::ButtonDownEvent(0,0))).Times(1);
    // TODO: Position on leave event
    EXPECT_CALL(*surface, consume(mt::PointerEventWithPosition(6, 6))).Times(1);
    EXPECT_CALL(*surface, consume(mt::ButtonUpEvent(6,6))).Times(1);
    EXPECT_CALL(*surface, consume(mt::PointerLeaveEvent())).Times(1);
    EXPECT_CALL(*another_surface, consume(mt::PointerEnterEvent())).Times(1);
    
    dispatcher.start();

    EXPECT_TRUE(dispatcher.dispatch(*ev_1));
    EXPECT_TRUE(dispatcher.dispatch(*ev_2));
    EXPECT_TRUE(dispatcher.dispatch(*ev_3));
}

TEST_F(DefaultInputDispatcher, pointer_gestures_may_transfer_over_buttons)
{
    using namespace ::testing;
    
    auto surface = scene.add_surface({{0, 0}, {5, 5}});
    auto another_surface = scene.add_surface({{5, 5}, {5, 5}});

    auto ev_1 = a_pointer_down_event(mir_pointer_button_primary);
    auto ev_2 = a_pointer_down_event(mir_pointer_button_secondary);
    auto ev_3 = a_pointer_up_event(mir_pointer_button_primary);
    auto ev_4 = a_pointer_event({6, 6});
    auto ev_5 = a_pointer_up_event(mir_pointer_button_secondary);

    InSequence seq;
    EXPECT_CALL(*surface, consume(mt::PointerEnterEvent())).Times(1);
    EXPECT_CALL(*surface, consume(mt::ButtonDownEvent(0,0))).Times(1);
    EXPECT_CALL(*surface, consume(mt::ButtonDownEvent(0,0))).Times(1);
    EXPECT_CALL(*surface, consume(mt::ButtonUpEvent(0,0))).Times(1);
    // TODO: Position on leave event
    EXPECT_CALL(*surface, consume(mt::PointerEventWithPosition(6, 6))).Times(1);
    EXPECT_CALL(*surface, consume(mt::PointerLeaveEvent())).Times(1);
    EXPECT_CALL(*another_surface, consume(mt::PointerEnterEvent())).Times(1);
    
    dispatcher.start();

    EXPECT_TRUE(dispatcher.dispatch(*ev_1));
    EXPECT_TRUE(dispatcher.dispatch(*ev_2));
    EXPECT_TRUE(dispatcher.dispatch(*ev_3));
    EXPECT_TRUE(dispatcher.dispatch(*ev_4));
    EXPECT_TRUE(dispatcher.dispatch(*ev_5));
}
// TODO: Test for what happens to gesture when client dissapears

// TODO: Add touch event builders
TEST_F(DefaultInputDispatcher, touch_delivered_to_surface)
{
    using namespace ::testing;
    
    auto surface = scene.add_surface({{1, 1}, {1, 1}});

    EXPECT_CALL(*surface, consume(mt::TouchEvent(0,0))).Times(1);
    EXPECT_CALL(*surface, consume(mt::TouchUpEvent(0,0))).Times(1);

    dispatcher.start();

    EXPECT_TRUE(dispatcher.dispatch(*a_touch_down_event({1,1})));
    EXPECT_TRUE(dispatcher.dispatch(*a_touch_up_event({1,1})));
}

TEST_F(DefaultInputDispatcher, touch_delivered_only_to_top_surface)
{
    using namespace ::testing;
    
    auto bottom_surface = scene.add_surface({{1, 1}, {3, 3}});
    auto surface = scene.add_surface({{1, 1}, {3, 3}});

    EXPECT_CALL(*surface, consume(mt::TouchEvent(0,0))).Times(1);
    EXPECT_CALL(*surface, consume(mt::TouchUpEvent(0,0))).Times(1);
    EXPECT_CALL(*bottom_surface, consume(mt::TouchEvent(0,0))).Times(0);
    EXPECT_CALL(*bottom_surface, consume(mt::TouchUpEvent(0,0))).Times(0);

    dispatcher.start();

    EXPECT_TRUE(dispatcher.dispatch(*a_touch_movement_event({1,1})));
    EXPECT_TRUE(dispatcher.dispatch(*a_touch_movement_event({2,2})));
}

// TODO: Test that touch can move between surfaces beside eachother

TEST_F(DefaultInputDispatcher, gestures_persist_over_touch_down)
{
    using namespace ::testing;
    
    auto left_surface = scene.add_surface({{0, 0}, {1, 1}});
    auto right_surface = scene.add_surface({{1, 1}, {1, 1}});

    EXPECT_CALL(*left_surface, consume(mt::TouchEvent(0, 0)));
    EXPECT_CALL(*left_surface, consume(mt::TouchMovementEvent(2, 2)));
    EXPECT_CALL(*left_surface, consume(mt::TouchUpEvent(2, 2)));
    EXPECT_CALL(*right_surface, consume(_)).Times(0);

    dispatcher.start();

    EXPECT_TRUE(dispatcher.dispatch(*a_touch_down_event({0, 0})));
    EXPECT_TRUE(dispatcher.dispatch(*a_touch_movement_event({2, 2})));
    EXPECT_TRUE(dispatcher.dispatch(*a_touch_up_event({2, 2})));
}
