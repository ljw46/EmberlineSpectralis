#pragma once

#include <juce_core/juce_core.h>
#include <memory>

namespace ember
{
    // A tiny, safe math-expression language used by the JSON wavetable definitions.
    //   numbers, + - * / % ^ ( ), comparisons < > <= >= == !=, logic && || !,
    //   constants pi tau e, the variables the caller declares, and the functions listed in docs/WAVETABLES.md
    //   (sin cos tanh abs pow min max clamp mix if frac odd even fib rand fm fold bend pd saw square tri ...).
    // Compiled once, then evaluated many times. No loops, no recursion, no side effects.
    class Expression
    {
    public:
        ~Expression();

        // `variables` are the names the expression may use; evaluate() takes their values in the same order.
        static std::unique_ptr<Expression> compile (const juce::String& text, const juce::StringArray& variables, juce::String& error);

        double evaluate (const double* values) const noexcept;

        static const juce::StringArray& functionNames();

        struct Node;   // implementation detail (public only so the parser in the .cpp can name it)

    private:
        explicit Expression (std::unique_ptr<Node> r);
        std::unique_ptr<Node> root;
    };
}
