#include "Expression.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace ember
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        double frac (double x) { return x - std::floor (x); }

        bool truthy (double v) { return std::abs (v) > 1.0e-12; }
        bool isZero (double v) { return std::abs (v) <= 1.0e-12; }

        double hashRandom (double seed, double i)
        {
            uint32_t h = (uint32_t) (int64_t) seed * 2654435761u + (uint32_t) (int64_t) i * 374761393u + 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            h ^= h >> 16;
            return (double) (h & 0xffffffu) / 16777216.0;
        }

        bool isInteger (double v) { return std::abs (v - std::round (v)) < 1.0e-9; }

        // J_n(x) for integer n >= 0, x >= 0, from J_n(x) = 1/(2 pi) * integral of cos(n t - x sin t) over a full period.
        // The integrand is periodic and analytic, so the trapezoid rule converges exponentially: with more points than
        // |x| + n (plus a margin) the error is far below double precision. libc++ (macOS) has no std::cyl_bessel_j, and
        // using one routine everywhere keeps the factory wavetables identical on every platform.
        double besselJPositive (int n, double x)
        {
            const int points = (int) std::ceil (x + (double) n + 32.0 + 10.0 * std::cbrt (x));
            const double step = 2.0 * 3.14159265358979323846 / (double) points;
            double sum = 0.0;
            for (int i = 0; i < points; ++i)
            {
                const double t = step * (double) i;
                sum += std::cos ((double) n * t - x * std::sin (t));
            }
            return sum / (double) points;
        }

        double besselJ (double order, double x)
        {
            if (! isInteger (order) || std::abs (order) > 200.0 || std::abs (x) > 1000.0)
                return 0.0;
            const int n = (int) std::round (order);
            const double j = besselJPositive (std::abs (n), std::abs (x));
            double v = (n < 0 && (n & 1)) ? -j : j;
            return x < 0.0 && (std::abs (n) & 1) ? -v : v;
        }

        // amplitude of harmonic h in two-operator FM (carrier 1, modulator `ratio`, index `index`)
        double fmAmplitude (double h, double ratio, double index)
        {
            if (ratio < 0.5) return 0.0;
            double sum = 0.0;
            for (double sign : { 1.0, -1.0 })
            {
                const double k = (sign * h - 1.0) / ratio;   // partial 1 + k*ratio equals +h or -h
                if (isInteger (k))
                    sum += sign * besselJ (k, index);
            }
            return sum;
        }

        bool isFibonacci (double h)
        {
            if (! isInteger (h) || h < 1.0) return false;
            double a = 1.0, b = 2.0;
            if (std::abs (h - 1.0) < 1.0e-9) return true;
            while (b < h - 0.5) { const double c = a + b; a = b; b = c; }
            return std::abs (b - h) < 1.0e-9;
        }

        bool isPrime (double h)
        {
            if (! isInteger (h) || h < 2.0) return false;
            const auto n = (int64_t) std::round (h);
            for (int64_t d = 2; d * d <= n; ++d)
                if (n % d == 0) return false;
            return true;
        }

        double foldTriangle (double x)
        {
            double p = x * 0.25 + 0.25;
            p -= std::floor (p);
            return 1.0 - 4.0 * std::abs (p - 0.5);
        }

        double bendPhase (double ph, double amount)
        {
            ph = frac (ph);
            double bent;
            if (ph < 0.5) { const double x = 2.0 * ph;        bent = 0.5 * x * x * x; }
            else          { const double y = 2.0 - 2.0 * ph;  bent = 1.0 - 0.5 * y * y * y; }
            return ph + amount * (bent - ph);
        }

        double phaseDistort (double ph, double amount)
        {
            ph = frac (ph);
            const double b = 0.5 - 0.49 * juce::jlimit (0.0, 1.0, amount);
            return ph < b ? 0.5 * ph / b : 0.5 + 0.5 * (ph - b) / (1.0 - b);
        }

        using Fn = double (*) (const double*);

        struct FnDef { const char* name; int arity; Fn fn; };

        const std::vector<FnDef>& functions()
        {
            static const std::vector<FnDef> list {
                { "sin",   1, [] (const double* a) { return std::sin (a[0]); } },
                { "cos",   1, [] (const double* a) { return std::cos (a[0]); } },
                { "tan",   1, [] (const double* a) { return std::tan (a[0]); } },
                { "tanh",  1, [] (const double* a) { return std::tanh (a[0]); } },
                { "atan",  1, [] (const double* a) { return std::atan (a[0]); } },
                { "abs",   1, [] (const double* a) { return std::abs (a[0]); } },
                { "sqrt",  1, [] (const double* a) { return a[0] < 0.0 ? 0.0 : std::sqrt (a[0]); } },
                { "exp",   1, [] (const double* a) { return std::exp (juce::jlimit (-60.0, 60.0, a[0])); } },
                { "log",   1, [] (const double* a) { return a[0] <= 0.0 ? 0.0 : std::log (a[0]); } },
                { "pow",   2, [] (const double* a) { const double r = std::pow (a[0], a[1]); return std::isfinite (r) ? r : 0.0; } },
                { "min",   2, [] (const double* a) { return std::min (a[0], a[1]); } },
                { "max",   2, [] (const double* a) { return std::max (a[0], a[1]); } },
                { "floor", 1, [] (const double* a) { return std::floor (a[0]); } },
                { "ceil",  1, [] (const double* a) { return std::ceil (a[0]); } },
                { "round", 1, [] (const double* a) { return std::round (a[0]); } },
                { "frac",  1, [] (const double* a) { return frac (a[0]); } },
                { "sign",  1, [] (const double* a) { return a[0] < 0.0 ? -1.0 : a[0] > 0.0 ? 1.0 : 0.0; } },
                { "clamp", 3, [] (const double* a) { return std::min (std::max (a[0], a[1]), a[2]); } },
                { "mix",   3, [] (const double* a) { return a[0] + (a[1] - a[0]) * a[2]; } },
                { "if",    3, [] (const double* a) { return truthy (a[0]) ? a[1] : a[2]; } },
                { "odd",   1, [] (const double* a) { return isInteger (a[0]) && ((int64_t) std::round (a[0]) & 1) ? 1.0 : 0.0; } },
                { "even",  1, [] (const double* a) { return isInteger (a[0]) && ! ((int64_t) std::round (a[0]) & 1) ? 1.0 : 0.0; } },
                { "prime", 1, [] (const double* a) { return isPrime (a[0]) ? 1.0 : 0.0; } },
                { "fib",   1, [] (const double* a) { return isFibonacci (a[0]) ? 1.0 : 0.0; } },
                { "rand",  2, [] (const double* a) { return hashRandom (a[0], a[1]); } },
                { "bessel",2, [] (const double* a) { return besselJ (a[0], a[1]); } },
                { "fm",    3, [] (const double* a) { return fmAmplitude (a[0], a[1], a[2]); } },
                { "fold",  1, [] (const double* a) { return foldTriangle (a[0]); } },
                { "bend",  2, [] (const double* a) { return bendPhase (a[0], a[1]); } },
                { "pd",    2, [] (const double* a) { return phaseDistort (a[0], a[1]); } },
                { "saw",   1, [] (const double* a) { return 2.0 * frac (a[0]) - 1.0; } },
                { "square",1, [] (const double* a) { return frac (a[0]) < 0.5 ? 1.0 : -1.0; } },
                { "tri",   1, [] (const double* a) { return 1.0 - 4.0 * std::abs (frac (a[0] + 0.25) - 0.5); } },
                { "pulse", 2, [] (const double* a) { return frac (a[0]) < a[1] ? 1.0 : -1.0; } },
            };
            return list;
        }

        enum class Op { add, sub, mul, div, mod, pow, lt, gt, le, ge, eq, ne, andOp, orOp, neg, notOp };
    }

    struct Expression::Node
    {
        enum class Kind { number, variable, unary, binary, call } kind = Kind::number;
        double value = 0.0;
        int index = 0;   // variable index or function index
        Op op = Op::add;
        std::vector<std::unique_ptr<Node>> kids;

        double eval (const double* v) const noexcept
        {
            switch (kind)
            {
                case Kind::number:   return value;
                case Kind::variable: return v[index];
                case Kind::unary:
                {
                    const double a = kids[0]->eval (v);
                    return op == Op::neg ? -a : (isZero (a) ? 1.0 : 0.0);
                }
                case Kind::binary:
                {
                    const double a = kids[0]->eval (v), b = kids[1]->eval (v);
                    switch (op)
                    {
                        case Op::add: return a + b;
                        case Op::sub: return a - b;
                        case Op::mul: return a * b;
                        case Op::div: return isZero (b) ? 0.0 : a / b;
                        case Op::mod: return isZero (b) ? 0.0 : std::fmod (a, b);
                        case Op::pow: { const double r = std::pow (a, b); return std::isfinite (r) ? r : 0.0; }
                        case Op::lt:  return a < b ? 1.0 : 0.0;
                        case Op::gt:  return a > b ? 1.0 : 0.0;
                        case Op::le:  return a <= b ? 1.0 : 0.0;
                        case Op::ge:  return a >= b ? 1.0 : 0.0;
                        case Op::eq:  return std::abs (a - b) < 1.0e-9 ? 1.0 : 0.0;
                        case Op::ne:  return std::abs (a - b) < 1.0e-9 ? 0.0 : 1.0;
                        case Op::andOp: return (truthy (a) && truthy (b)) ? 1.0 : 0.0;
                        case Op::orOp:  return (truthy (a) || truthy (b)) ? 1.0 : 0.0;
                        case Op::neg:
                        case Op::notOp: return 0.0;   // unary only
                    }
                }
                case Kind::call:
                {
                    double args[3] { 0, 0, 0 };
                    for (size_t i = 0; i < kids.size(); ++i)
                        args[i] = kids[i]->eval (v);
                    return functions()[(size_t) index].fn (args);
                }
            }
            return 0.0;
        }
    };

    namespace
    {
        // recursive-descent parser
        class Parser
        {
        public:
            Parser (const juce::String& t, const juce::StringArray& vars) : text (t), variables (vars) {}

            using NodePtr = std::unique_ptr<Expression::Node>;

            NodePtr parseAll()
            {
                auto n = parseOr (0);
                skipSpace();
                if (n && pos < text.length())
                    fail ("unexpected '" + juce::String::charToString (text[pos]) + "'");
                return error.isEmpty() ? std::move (n) : nullptr;
            }

            juce::String error;

        private:
            static constexpr int maxDepth = 40;

            void fail (const juce::String& m) { if (error.isEmpty()) error = m + " (at character " + juce::String (pos + 1) + ")"; }

            void skipSpace() { while (pos < text.length() && juce::CharacterFunctions::isWhitespace (text[pos])) ++pos; }

            bool take (const char* s)
            {
                skipSpace();
                const int n = (int) std::strlen (s);
                if (text.substring (pos, pos + n) == s) { pos += n; return true; }
                return false;
            }

            NodePtr make (Expression::Node::Kind k)
            {
                auto n = std::make_unique<Expression::Node>();
                n->kind = k;
                return n;
            }

            NodePtr binary (Op op, NodePtr a, NodePtr b)
            {
                if (! a || ! b) return nullptr;
                auto n = make (Expression::Node::Kind::binary);
                n->op = op;
                n->kids.push_back (std::move (a));
                n->kids.push_back (std::move (b));
                return n;
            }

            NodePtr parseOr (int depth)
            {
                auto a = parseAnd (depth);
                while (error.isEmpty() && take ("||")) a = binary (Op::orOp, std::move (a), parseAnd (depth));
                return a;
            }

            NodePtr parseAnd (int depth)
            {
                auto a = parseCompare (depth);
                while (error.isEmpty() && take ("&&")) a = binary (Op::andOp, std::move (a), parseCompare (depth));
                return a;
            }

            NodePtr parseCompare (int depth)
            {
                auto a = parseAdd (depth);
                if (! error.isEmpty()) return a;
                if (take ("<="))      return binary (Op::le, std::move (a), parseAdd (depth));
                if (take (">="))      return binary (Op::ge, std::move (a), parseAdd (depth));
                if (take ("=="))      return binary (Op::eq, std::move (a), parseAdd (depth));
                if (take ("!="))      return binary (Op::ne, std::move (a), parseAdd (depth));
                if (take ("<"))       return binary (Op::lt, std::move (a), parseAdd (depth));
                if (take (">"))       return binary (Op::gt, std::move (a), parseAdd (depth));
                return a;
            }

            NodePtr parseAdd (int depth)
            {
                auto a = parseMul (depth);
                while (error.isEmpty())
                {
                    if (take ("+"))      a = binary (Op::add, std::move (a), parseMul (depth));
                    else if (take ("-")) a = binary (Op::sub, std::move (a), parseMul (depth));
                    else break;
                }
                return a;
            }

            NodePtr parseMul (int depth)
            {
                auto a = parseUnary (depth);
                while (error.isEmpty())
                {
                    if (take ("*"))      a = binary (Op::mul, std::move (a), parseUnary (depth));
                    else if (take ("/")) a = binary (Op::div, std::move (a), parseUnary (depth));
                    else if (take ("%")) a = binary (Op::mod, std::move (a), parseUnary (depth));
                    else break;
                }
                return a;
            }

            NodePtr parseUnary (int depth)
            {
                if (depth > maxDepth) { fail ("expression is nested too deeply"); return nullptr; }
                skipSpace();
                if (take ("-")) { auto n = make (Expression::Node::Kind::unary); n->op = Op::neg; n->kids.push_back (parseUnary (depth + 1)); return n->kids[0] ? std::move (n) : nullptr; }
                if (take ("!")) { auto n = make (Expression::Node::Kind::unary); n->op = Op::notOp; n->kids.push_back (parseUnary (depth + 1)); return n->kids[0] ? std::move (n) : nullptr; }
                auto base = parsePrimary (depth + 1);
                if (error.isEmpty() && take ("^"))
                    return binary (Op::pow, std::move (base), parseUnary (depth + 1));   // right-associative
                return base;
            }

            NodePtr parsePrimary (int depth)
            {
                skipSpace();
                if (pos >= text.length()) { fail ("unexpected end of expression"); return nullptr; }

                if (take ("("))
                {
                    auto n = parseOr (depth);
                    if (! take (")")) fail ("missing ')'");
                    return n;
                }

                const auto c = text[pos];
                if (juce::CharacterFunctions::isDigit (c) || c == '.')
                {
                    const int start = pos;
                    while (pos < text.length() && (juce::CharacterFunctions::isDigit (text[pos]) || text[pos] == '.')) ++pos;
                    if (pos < text.length() && (text[pos] == 'e' || text[pos] == 'E')
                        && (pos + 1 < text.length() && (juce::CharacterFunctions::isDigit (text[pos + 1]) || text[pos + 1] == '-' || text[pos + 1] == '+')))
                    {
                        pos += 2;
                        while (pos < text.length() && juce::CharacterFunctions::isDigit (text[pos])) ++pos;
                    }
                    auto n = make (Expression::Node::Kind::number);
                    n->value = text.substring (start, pos).getDoubleValue();
                    return n;
                }

                if (juce::CharacterFunctions::isLetter (c) || c == '_')
                {
                    const int start = pos;
                    while (pos < text.length() && (juce::CharacterFunctions::isLetterOrDigit (text[pos]) || text[pos] == '_')) ++pos;
                    const auto name = text.substring (start, pos);

                    if (take ("("))
                    {
                        const auto& fns = functions();
                        int idx = -1;
                        for (size_t i = 0; i < fns.size(); ++i)
                            if (name == fns[i].name) idx = (int) i;
                        if (idx < 0) { fail ("unknown function '" + name + "'"); return nullptr; }

                        auto n = make (Expression::Node::Kind::call);
                        n->index = idx;
                        if (! take (")"))
                        {
                            do { n->kids.push_back (parseOr (depth)); if (! error.isEmpty()) return nullptr; } while (take (","));
                            if (! take (")")) { fail ("missing ')' after the arguments of " + name); return nullptr; }
                        }
                        if ((int) n->kids.size() != fns[(size_t) idx].arity)
                        {
                            fail (name + "() takes " + juce::String (fns[(size_t) idx].arity) + " argument(s), not " + juce::String ((int) n->kids.size()));
                            return nullptr;
                        }
                        return n;
                    }

                    if (name == "pi" || name == "tau" || name == "e")
                    {
                        auto n = make (Expression::Node::Kind::number);
                        n->value = name == "pi" ? pi : name == "tau" ? 2.0 * pi : 2.718281828459045;
                        return n;
                    }

                    const int v = variables.indexOf (name);
                    if (v < 0)
                    {
                        fail ("unknown name '" + name + "' (available here: " + variables.joinIntoString (", ") + ")");
                        return nullptr;
                    }
                    auto n = make (Expression::Node::Kind::variable);
                    n->index = v;
                    return n;
                }

                fail ("unexpected '" + juce::String::charToString (c) + "'");
                return nullptr;
            }

            juce::String text;
            juce::StringArray variables;
            int pos = 0;
        };
    }

    Expression::Expression (std::unique_ptr<Node> r) : root (std::move (r)) {}
    Expression::~Expression() = default;

    std::unique_ptr<Expression> Expression::compile (const juce::String& text, const juce::StringArray& variables, juce::String& error)
    {
        if (text.trim().isEmpty()) { error = "empty expression"; return nullptr; }
        if (text.length() > 4000) { error = "expression is too long"; return nullptr; }

        Parser parser (text, variables);
        auto root = parser.parseAll();
        if (root == nullptr)
        {
            error = parser.error.isEmpty() ? juce::String ("could not parse expression") : parser.error;
            return nullptr;
        }
        return std::unique_ptr<Expression> (new Expression (std::move (root)));
    }

    double Expression::evaluate (const double* values) const noexcept
    {
        const double r = root->eval (values);
        return std::isfinite (r) ? r : 0.0;
    }

    const juce::StringArray& Expression::functionNames()
    {
        static const juce::StringArray names = [] { juce::StringArray n; for (auto& f : functions()) n.add (juce::String (f.name) + "/" + juce::String (f.arity)); return n; }();
        return names;
    }
}
