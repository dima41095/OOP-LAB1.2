#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <sstream>
#include <clocale>
#include <cmath>
#include <algorithm>

// ==================== Узел дерева выражения ====================
template <typename T>
class Node {
public:
    virtual ~Node() = default;
    virtual T eval(const std::vector<std::pair<std::string, T>>& vars) const = 0;
    virtual std::string str() const = 0;
    virtual void collectVars(std::set<std::string>& out) const = 0;
    virtual int depth() const = 0;
    virtual int count() const = 0;
    virtual std::shared_ptr<Node<T>> simplify() const = 0;
    virtual bool isConst() const = 0;
    virtual T constValue() const = 0;
};

// ==================== Числовая константа ====================
template <typename T>
class ConstNode : public Node<T> {
    T value_;
public:
    explicit ConstNode(T v) : value_(v) {}
    T eval(const std::vector<std::pair<std::string, T>>&) const override { return value_; }
    std::string str() const override {
        std::ostringstream os;
        os << value_;
        return os.str();
    }
    void collectVars(std::set<std::string>&) const override {}
    int depth() const override { return 1; }
    int count() const override { return 1; }
    std::shared_ptr<Node<T>> simplify() const override {
        return std::make_shared<ConstNode<T>>(value_);
    }
    bool isConst() const override { return true; }
    T constValue() const override { return value_; }
};

// ==================== Именованная переменная ====================
template <typename T>
class VarNode : public Node<T> {
    std::string name_;
public:
    explicit VarNode(std::string name) : name_(std::move(name)) {}
    T eval(const std::vector<std::pair<std::string, T>>& vars) const override {
        for (const auto& p : vars)
            if (p.first == name_) return p.second;
        throw std::runtime_error("Неизвестная переменная: " + name_);
    }
    std::string str() const override { return name_; }
    void collectVars(std::set<std::string>& out) const override { out.insert(name_); }
    int depth() const override { return 1; }
    int count() const override { return 1; }
    std::shared_ptr<Node<T>> simplify() const override {
        return std::make_shared<VarNode<T>>(name_);
    }
    bool isConst() const override { return false; }
    T constValue() const override {
        throw std::logic_error("Переменная не является константой");
    }
};

// ==================== Бинарная операция ====================
template <typename T>
class BinaryNode : public Node<T> {
public:
    enum class Op { Add, Sub, Mul, Div };
private:
    Op op_;
    std::shared_ptr<Node<T>> left_, right_;

    static T apply(Op op, T a, T b) {
        switch (op) {
        case Op::Add: return a + b;
        case Op::Sub: return a - b;
        case Op::Mul: return a * b;
        case Op::Div:
            if (b == T(0)) throw std::runtime_error("Деление на ноль");
            return a / b;
        }
        throw std::logic_error("Неизвестная операция");
    }

    static std::string opStr(Op op) {
        switch (op) {
        case Op::Add: return "+";
        case Op::Sub: return "-";
        case Op::Mul: return "*";
        case Op::Div: return "/";
        }
        return "?";
    }
public:
    BinaryNode(Op op, std::shared_ptr<Node<T>> l, std::shared_ptr<Node<T>> r)
        : op_(op), left_(std::move(l)), right_(std::move(r)) {
        if (!left_ || !right_)
            throw std::invalid_argument("Операнды бинарной операции не могут быть пустыми");
    }
    T eval(const std::vector<std::pair<std::string, T>>& vars) const override {
        return apply(op_, left_->eval(vars), right_->eval(vars));
    }
    std::string str() const override {
        return "(" + left_->str() + " " + opStr(op_) + " " + right_->str() + ")";
    }
    void collectVars(std::set<std::string>& out) const override {
        left_->collectVars(out);
        right_->collectVars(out);
    }
    int depth() const override {
        return 1 + std::max(left_->depth(), right_->depth());
    }
    int count() const override {
        return 1 + left_->count() + right_->count();
    }
    std::shared_ptr<Node<T>> simplify() const override {
        auto l = left_->simplify();
        auto r = right_->simplify();
        if (l->isConst() && r->isConst())
            return std::make_shared<ConstNode<T>>(apply(op_, l->constValue(), r->constValue()));
        return std::make_shared<BinaryNode<T>>(op_, l, r);
    }
    bool isConst() const override { return false; }
    T constValue() const override {
        throw std::logic_error("Выражение не является константой");
    }
};

// ==================== Фабричные функции ====================
template <typename T>
std::shared_ptr<Node<T>> makeConst(T v) {
    return std::make_shared<ConstNode<T>>(v);
}

template <typename T>
std::shared_ptr<Node<T>> makeVar(const std::string& name) {
    return std::make_shared<VarNode<T>>(name);
}

template <typename T>
std::shared_ptr<Node<T>> makeAdd(std::shared_ptr<Node<T>> l, std::shared_ptr<Node<T>> r) {
    return std::make_shared<BinaryNode<T>>(BinaryNode<T>::Op::Add, l, r);
}

template <typename T>
std::shared_ptr<Node<T>> makeSub(std::shared_ptr<Node<T>> l, std::shared_ptr<Node<T>> r) {
    return std::make_shared<BinaryNode<T>>(BinaryNode<T>::Op::Sub, l, r);
}

template <typename T>
std::shared_ptr<Node<T>> makeMul(std::shared_ptr<Node<T>> l, std::shared_ptr<Node<T>> r) {
    return std::make_shared<BinaryNode<T>>(BinaryNode<T>::Op::Mul, l, r);
}

template <typename T>
std::shared_ptr<Node<T>> makeDiv(std::shared_ptr<Node<T>> l, std::shared_ptr<Node<T>> r) {
    return std::make_shared<BinaryNode<T>>(BinaryNode<T>::Op::Div, l, r);
}

// ==================== Демонстрация ====================
template <typename T>
void demo(const std::string& title, const std::shared_ptr<Node<T>>& expr,
    const std::vector<std::pair<std::string, T>>& vars) {
    std::cout << "\n--- " << title << " ---\n";
    std::cout << "Выражение: " << expr->str() << "\n";
    std::cout << "Глубина: " << expr->depth() << "\n";
    std::cout << "Количество узлов: " << expr->count() << "\n";
    std::set<std::string> used;
    expr->collectVars(used);
    std::cout << "Используемые переменные: ";
    for (const auto& v : used) std::cout << v << " ";
    std::cout << "\n";
    if (!vars.empty()) {
        std::cout << "Значение: " << expr->eval(vars) << "\n";
    }
    auto simplified = expr->simplify();
    std::cout << "Упрощённое выражение: " << simplified->str() << "\n";
}

// ==================== Интерактивная демонстрация ====================
template <typename T>
void interactive(const std::shared_ptr<Node<T>>& expr, const std::string& title) {
    std::cout << "\n=== Ручная демонстрация: " << title << " ===\n";
    std::cout << "Выражение: " << expr->str() << "\n";

    std::set<std::string> used;
    expr->collectVars(used);

    if (used.empty()) {
        std::cout << "Выражение не содержит переменных. Значение: "
            << expr->eval({}) << "\n";
        return;
    }

    std::cout << "Переменные: ";
    for (const auto& v : used) std::cout << v << " ";
    std::cout << "\nВведите значения переменных (пустая строка — выход):\n";

    while (true) {
        std::vector<std::pair<std::string, T>> vars;
        bool ok = true;
        for (const auto& name : used) {
            std::cout << "  " << name << " = ";
            std::string line;
            if (!std::getline(std::cin, line)) return;
            if (line.empty()) return;
            try {
                std::size_t pos = 0;
                T val = static_cast<T>(std::stod(line, &pos));
                if (pos != line.size()) throw std::invalid_argument("хвост");
                vars.emplace_back(name, val);
            }
            catch (...) {
                std::cout << "  Некорректное число, попробуйте снова.\n";
                ok = false;
                break;
            }
        }
        if (!ok) continue;
        try {
            std::cout << "  Значение: " << expr->eval(vars) << "\n";
        }
        catch (const std::exception& e) {
            std::cout << "  Ошибка: " << e.what() << "\n";
        }
        std::cout << "  (Enter — ещё раз, пустая строка в поле переменной — выход)\n";
    }
}

// ==================== Тесты ====================
int main() {
    std::setlocale(LC_ALL, "Russian");

    using T = double;

    try {
        auto onlyConst = makeMul<T>(
            makeAdd<T>(makeConst<T>(2), makeConst<T>(3)),
            makeConst<T>(4));
        auto oneVar = makeMul<T>(
            makeAdd<T>(makeConst<T>(2), makeConst<T>(3)),
            makeVar<T>("price"));
        auto multiVar = makeAdd<T>(
            makeMul<T>(makeVar<T>("price"), makeVar<T>("count")),
            makeVar<T>("delivery"));
        auto deep = makeDiv<T>(
            makeMul<T>(
                makeAdd<T>(makeVar<T>("a"), makeVar<T>("b")),
                makeSub<T>(makeVar<T>("c"), makeConst<T>(1))),
            makeConst<T>(2));

        std::cout << "########## АВТОМАТИЧЕСКИЕ ТЕСТЫ ##########\n";
        demo<T>("Только константы", onlyConst, {});
        demo<T>("Одна переменная", oneVar, { {"price", 10} });
        demo<T>("Несколько переменных (вариант 1)", multiVar,
            { {"price", 100}, {"count", 3}, {"delivery", 50} });
        demo<T>("Несколько переменных (вариант 2)", multiVar,
            { {"price", 200}, {"count", 2}, {"delivery", 0} });
        demo<T>("Многоуровневое выражение", deep,
            { {"a", 4}, {"b", 6}, {"c", 5} });

        std::cout << "\n=== Обработка ошибок ===\n";
        try {
            std::cout << "Значение: " << multiVar->eval({ {"price", 10} }) << "\n";
        }
        catch (const std::exception& e) {
            std::cout << "Перехвачено: " << e.what() << "\n";
        }
        try {
            auto divByZero = makeDiv<T>(makeConst<T>(1), makeConst<T>(0));
            std::cout << "Значение: " << divByZero->eval({}) << "\n";
        }
        catch (const std::exception& e) {
            std::cout << "Перехвачено: " << e.what() << "\n";
        }

        std::cout << "\n########## РУЧНАЯ ДЕМОНСТРАЦИЯ ##########\n";
        std::cout << "Доступные выражения:\n"
            << "  1 — только константы\n"
            << "  2 — одна переменная (price)\n"
            << "  3 — несколько переменных (price, count, delivery)\n"
            << "  4 — многоуровневое (a, b, c)\n"
            << "  0 — выход\n";

        while (true) {
            std::cout << "\nВыберите выражение: ";
            std::string choice;
            if (!std::getline(std::cin, choice)) break;
            if (choice == "0") break;
            else if (choice == "1") interactive<T>(onlyConst, "только константы");
            else if (choice == "2") interactive<T>(oneVar, "одна переменная");
            else if (choice == "3") interactive<T>(multiVar, "несколько переменных");
            else if (choice == "4") interactive<T>(deep, "многоуровневое");
            else std::cout << "Неизвестный выбор.\n";
        }

        std::cout << "Завершение работы.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << "\n";
        return 1;
    }
    return 0;
}