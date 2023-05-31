#pragma once

#include "raylib.h"

namespace ViewExplorer
{
    enum RotaryAxis
    {
        Pitch = 0,
        Roll = 1,
        Yaw = 2
    };

    enum EAction
    {
        MoveNot = 0,
        MoveOne = 1,
        MoveTwo = 2
    };

    struct RotaryAxes : Vector3
    {
        RotaryAxes() : Vector3{ 0.f, 0.f, 0.f } {} // Set pitch roll and yaw to zero

        inline float& operator [] (RotaryAxis rot)
        {
            if (rot == Pitch) return x;
            if (rot == Roll) return y;
            return z;
        }

        void HandleView()
        {
            auto& axes = *this;

            HandlePitch(axes, PitchState());
            HandleRoll(axes, RollState());
            HandleYaw(axes, YawState());
        }

        static void HandlePitch(RotaryAxes& axes, EAction action)
        {
            if (action == MoveOne) axes[Pitch] += 0.6f;
            else if (action == MoveTwo) axes[Pitch] -= 0.6f;
            else
            {
                if (axes[Pitch] > 0.3f) axes[Pitch] -= 0.3f;
                else if (axes[Pitch] < -0.3f) axes[Pitch] += 0.3f;
            }
        }

        static void HandleRoll(RotaryAxes& axes, EAction action)
        {
            if (action == MoveOne) axes[Roll] -= 1.0f;
            else if (action == MoveTwo) axes[Roll] += 1.0f;
            else
            {
                if (axes[Roll] > 0.0f) axes[Roll] -= 0.5f;
                else if (axes[Roll] < 0.0f) axes[Roll] += 0.5f;
            }
        }

        static void HandleYaw(RotaryAxes& axes, EAction action)
        {
            if (action == MoveOne) axes[Yaw] -= 1.0f;
            else if (action == MoveTwo) axes[Yaw] += 1.0f;
            else
            {
                if (axes[Yaw] > 0.0f) axes[Yaw] -= 0.5f;
                else if (axes[Yaw] < 0.0f) axes[Yaw] += 0.5f;
            }
        }

        static EAction PitchState()
        {
            if (IsKeyDown(KEY_DOWN)) return MoveOne;
            if (IsKeyDown(KEY_UP)) return MoveTwo;
            return MoveNot;
        }

        static EAction RollState()
        {
            if (IsKeyDown(KEY_LEFT)) return MoveOne;
            if (IsKeyDown(KEY_RIGHT)) return MoveTwo;
            return MoveNot;
        }

        static EAction YawState()
        {
            if (IsKeyDown(KEY_S)) return MoveOne;
            if (IsKeyDown(KEY_A)) return MoveTwo;
            return MoveNot;
        }

        Vector3 Transformer()
        {
            auto& axes = *this;
            return Vector3 { DEG2RAD * axes[Pitch], DEG2RAD * axes[Yaw], DEG2RAD * axes[Roll] };
        }
    };

}