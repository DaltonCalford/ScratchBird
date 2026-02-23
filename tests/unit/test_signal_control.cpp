/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/signal_control.h"

#include <gtest/gtest.h>

namespace
{

using scratchbird::core::ControlSignal;
using scratchbird::core::SignalInstallSpec;
using scratchbird::core::Status;

TEST(SignalControlTest, InjectAndPollReturnsSignalThenClears)
{
    auto control = scratchbird::core::createDefaultSignalControl();
    ASSERT_NE(control, nullptr);

    SignalInstallSpec spec;
    ASSERT_EQ(control->install(spec, nullptr), Status::OK);
    ASSERT_EQ(control->inject(ControlSignal::RELOAD, nullptr), Status::OK);

    ControlSignal first = ControlSignal::NONE;
    ASSERT_EQ(control->poll(&first, nullptr), Status::OK);
    EXPECT_EQ(first, ControlSignal::RELOAD);

    ControlSignal second = ControlSignal::RELOAD;
    ASSERT_EQ(control->poll(&second, nullptr), Status::OK);
    EXPECT_EQ(second, ControlSignal::NONE);

    ASSERT_EQ(control->uninstall(nullptr), Status::OK);
}

TEST(SignalControlTest, RejectsNoneInjection)
{
    auto control = scratchbird::core::createDefaultSignalControl();
    ASSERT_NE(control, nullptr);
    ASSERT_EQ(control->install(SignalInstallSpec{}, nullptr), Status::OK);

    EXPECT_EQ(control->inject(ControlSignal::NONE, nullptr), Status::INVALID_ARGUMENT);

    ASSERT_EQ(control->uninstall(nullptr), Status::OK);
}

TEST(SignalControlTest, PollRejectsNullOutput)
{
    auto control = scratchbird::core::createDefaultSignalControl();
    ASSERT_NE(control, nullptr);
    ASSERT_EQ(control->install(SignalInstallSpec{}, nullptr), Status::OK);

    EXPECT_EQ(control->poll(nullptr, nullptr), Status::INVALID_ARGUMENT);

    ASSERT_EQ(control->uninstall(nullptr), Status::OK);
}

} // namespace
