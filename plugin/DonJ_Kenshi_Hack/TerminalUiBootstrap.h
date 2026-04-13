#pragma once

namespace DonJTerminalUiBootstrap
{
    enum State
    {
        NotCreated,
        Created,
        Failed
    };

    enum Decision
    {
        Skip,
        AttemptCreate,
        WindowAlreadyPresent,
        GuiUnavailable
    };

    enum ToggleDecision
    {
        IgnoreToggle,
        CreateAndShowWindow,
        ShowExistingWindow,
        HideWindow
    };

    struct Context
    {
        bool hasTitleScreen;
        bool hasGui;
        bool hasWindow;
        bool isGameWorldReady;

        Context()
            : hasTitleScreen(false)
            , hasGui(false)
            , hasWindow(false)
            , isGameWorldReady(false)
        {
        }

        Context(
            bool hasTitleScreenValue,
            bool hasGuiValue,
            bool hasWindowValue,
            bool isGameWorldReadyValue)
            : hasTitleScreen(hasTitleScreenValue)
            , hasGui(hasGuiValue)
            , hasWindow(hasWindowValue)
            , isGameWorldReady(isGameWorldReadyValue)
        {
        }
    };

    inline Decision Evaluate(const Context& context, State state)
    {
        if (context.hasWindow || state == Created)
        {
            return WindowAlreadyPresent;
        }

        if (!context.hasTitleScreen || context.isGameWorldReady)
        {
            return Skip;
        }

        if (!context.hasGui)
        {
            return GuiUnavailable;
        }

        if (state == Failed)
        {
            return Skip;
        }

        return AttemptCreate;
    }

    struct ToggleContext
    {
        bool toggleRequested;
        bool hasGui;
        bool hasWindow;
        bool isWindowVisible;
        bool isGameWorldReady;

        ToggleContext()
            : toggleRequested(false)
            , hasGui(false)
            , hasWindow(false)
            , isWindowVisible(false)
            , isGameWorldReady(false)
        {
        }

        ToggleContext(
            bool toggleRequestedValue,
            bool hasGuiValue,
            bool hasWindowValue,
            bool isWindowVisibleValue,
            bool isGameWorldReadyValue)
            : toggleRequested(toggleRequestedValue)
            , hasGui(hasGuiValue)
            , hasWindow(hasWindowValue)
            , isWindowVisible(isWindowVisibleValue)
            , isGameWorldReady(isGameWorldReadyValue)
        {
        }
    };

    inline ToggleDecision EvaluateToggle(const ToggleContext& context)
    {
        if (!context.toggleRequested)
        {
            return IgnoreToggle;
        }

        if (context.isWindowVisible)
        {
            return HideWindow;
        }

        if (context.hasWindow)
        {
            return ShowExistingWindow;
        }

        if (!context.hasGui)
        {
            return IgnoreToggle;
        }

        return CreateAndShowWindow;
    }

    inline bool ShouldCaptureKeyboard(bool hasWindow, bool isWindowVisible)
    {
        return hasWindow && isWindowVisible;
    }
}
