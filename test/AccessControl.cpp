/*
    Copyright (C) 2014-2016 Islog

    This file is part of Leosac.

    Leosac is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Leosac is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "core/auth/AuthTarget.hpp"
#include "core/auth/Interfaces/IUser.hpp"
#include "core/auth/SimpleAccessProfile.hpp"
#include "core/auth/WiegandCard.hpp"
#include "tools/Schedule.hpp"
#include <chrono>
#include <gtest/gtest.h>

using namespace Leosac::Auth;
namespace Leosac
{
namespace Test
{
class AccessControlTest : public ::testing::Test
{
  public:
    AccessControlTest()
        : user(new IUser("my_uuid"))
        , profile(new SimpleAccessProfile())
        , auth_source_(new BaseAuthSource())
        , card(new WiegandCard("01:f2:fe:23", 32))
        , target(new AuthTarget("random_target"))
    {
        // this is authentication module work
        card->owner(user);
        auth_source_->addAuthSource(card);
        auth_source_->owner(user);
        user->profile(profile);

        // add access monday from 10 to 14.
        auto profile_r = std::dynamic_pointer_cast<SimpleAccessProfile>(profile);
        Tools::Schedule s;
        Tools::SingleTimeFrame tf;
        tf.day        = 1;
        tf.start_hour = 10;
        tf.start_min  = 0;
        tf.end_hour   = 14;
        tf.end_min    = 0;
        s.add_timeframe(tf);

        profile_r->addAccessSchedule(target, s);
    }

    ~AccessControlTest()
    {
    }

    IUserPtr user;
    IAccessProfilePtr profile;
    IAuthenticationSourcePtr auth_source_;
    WiegandCardPtr card;
    AuthTargetPtr target;
};

TEST_F(AccessControlTest, Bla)
{
    IAccessProfilePtr p;

    p = user->profile();
    ASSERT_TRUE(p.get());

    // Monday
    std::tm date            = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    date.tm_hour            = 12;
    date.tm_mday            = 3;
    date.tm_mon             = 10;
    date.tm_year            = 114;
    std::time_t time_temp   = std::mktime(&date);
    std::tm const *time_out = std::localtime(&time_temp);
    ASSERT_EQ(1, time_out->tm_wday);

    ASSERT_TRUE(p->isAccessGranted(std::chrono::system_clock::from_time_t(time_temp),
                                   target));

    std::tm cpy            = *time_out;
    cpy.tm_hour            = 14;
    cpy.tm_min             = 20;
    std::time_t time_temp2 = std::mktime(&cpy);
    ASSERT_FALSE(p->isAccessGranted(
        std::chrono::system_clock::from_time_t(time_temp2), target));
}
}
}
