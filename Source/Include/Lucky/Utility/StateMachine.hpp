#pragma once

#include <map>
#include <memory>
#include <stdint.h>
#include <typeindex>

namespace Lucky
{
    template <typename TargetType>
    struct StateMachine;

    template <typename TargetType>
    struct State
    {
        void Setup(StateMachine<TargetType> *stateMachine, TargetType *target)
        {
            this->stateMachine = stateMachine;
            this->target = target;
        }

        virtual void Enter()
        {
        }
        virtual void Exit()
        {
        }

        virtual void BeforeUpdate()
        {
        }
        virtual void Update(float deltaSeconds)
        {
        }
        virtual void AfterUpdate()
        {
        }

        StateMachine<TargetType> *stateMachine = nullptr;
        TargetType *target = nullptr;
    };

    template <typename TargetType>
    struct StateMachine
    {
        StateMachine(TargetType *target)
            : target(target)
        {
        }

        template <typename StateType, typename... Args>
        void Add(Args... args)
        {
            auto &stateId = typeid(StateType);
            auto state = std::make_unique<StateType>(args...);

            state->Setup(this, target);
            states.emplace(stateId, std::move(state));
        }

        template <typename StateType>
        void SetState(bool allowRestart = false)
        {
            auto &stateId = typeid(StateType);
            auto targetState = states[stateId].get();

            if (!allowRestart && currentState == targetState)
                return;

            if (currentState != nullptr)
                currentState->Exit();

            previousStateType = currentStateType;
            currentStateType = stateId;

            currentState = targetState;
            currentState->Enter();
        }

        void Update(float deltaSeconds)
        {
            if (currentState == nullptr)
                return;

            currentState->BeforeUpdate();
            currentState->Update(deltaSeconds);
            currentState->AfterUpdate();
        }

        std::map<std::type_index, std::unique_ptr<State<TargetType>>> states;
        State<TargetType> *currentState = nullptr;
        std::type_index currentStateType = typeid(nullptr_t);
        std::type_index previousStateType = typeid(nullptr_t);
        TargetType *target = nullptr;
    };
} // namespace Lucky
