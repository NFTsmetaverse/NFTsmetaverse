#pragma once

#include "Misc/AutomationTest.h"

/**
 * The flags every suite in this module registers with, in one place.
 *
 * EAutomationTestFlags has already changed shape once in UE 5's lifetime -- it was
 * a namespace of integer constants and became an enum class with flag operators --
 * and this project is being opened on an engine several releases newer than the one
 * it was written against. Spelling the expression out in each of the nine suites
 * would mean fifty-five edits if it has moved again. This way it is one.
 *
 * EditorContext because these run in-editor rather than in a cooked build, and
 * EngineFilter to group them with engine-level tests rather than smoke or
 * product tests.
 */
#define CHAIN_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
