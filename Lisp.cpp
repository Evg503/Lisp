// Lisp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file


#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>
#include <algorithm>
#include <sstream>
#include <optional>

// ========== Типы данных ==========
struct Symbol {
    std::string name;
    bool operator==(const Symbol& other) const { return name == other.name; }
};

// Forward declaration
struct Object;
class Environment;

using ObjectPtr = std::shared_ptr<Object>;
using ObjectList = std::vector<ObjectPtr>;

// Типы Lisp объектов
struct Nil {};  // пустой список
struct Integer { long long value; };
struct Boolean { bool value; };
struct String { std::string value; };

// Функция (замыкание)
struct Function {
    std::vector<std::string> params;     // имена параметров
    ObjectPtr body;                       // тело функции
    std::shared_ptr<Environment> env;     // окружение захвата
};

struct Object {
    std::variant<
        Nil,
        Integer,
        Boolean,
        String,
        Symbol,
        ObjectList,
        Function
    > data;

    Object(Nil n) : data(n) {}
    Object(Integer i) : data(i) {}
    Object(Boolean b) : data(b) {}
    Object(String s) : data(s) {}
    Object(Symbol sym) : data(sym) {}
    Object(ObjectList list) : data(list) {}
    Object(Function func) : data(func) {}

    bool isNil() const { return std::holds_alternative<Nil>(data); }
    bool isInteger() const { return std::holds_alternative<Integer>(data); }
    bool isBoolean() const { return std::holds_alternative<Boolean>(data); }
    bool isString() const { return std::holds_alternative<String>(data); }
    bool isSymbol() const { return std::holds_alternative<Symbol>(data); }
    bool isList() const { return std::holds_alternative<ObjectList>(data); }
    bool isFunction() const { return std::holds_alternative<Function>(data); }

    // Геттеры с проверкой
    Integer& asInteger() { return std::get<Integer>(data); }
    Boolean& asBoolean() { return std::get<Boolean>(data); }
    String& asString() { return std::get<String>(data); }
    Symbol& asSymbol() { return std::get<Symbol>(data); }
    ObjectList& asList() { return std::get<ObjectList>(data); }
    Function& asFunction() { return std::get<Function>(data); }
};

// ========== Окружение ==========
class Environment : public std::enable_shared_from_this<Environment> {
public:
    std::shared_ptr<Environment> parent;
    std::unordered_map<std::string, ObjectPtr> symbols;

    Environment(std::shared_ptr<Environment> parent = nullptr) : parent(parent) {}

    void define(const std::string& name, ObjectPtr value) {
        symbols[name] = value;
    }

    ObjectPtr find(const std::string& name) {
        if (symbols.contains(name)) {
            return symbols[name];
        }
        if (parent) {
            return parent->find(name);
        }
        return std::make_shared<Object>(Nil{});
    }

    std::shared_ptr<Environment> extend() {
        return std::make_shared<Environment>(shared_from_this());
    }
};

// ========== Парсер ==========
class Tokenizer {
    std::string input;
    size_t pos = 0;

public:
    Tokenizer(const std::string& src) : input(src) {}

    std::vector<std::string> tokenize() {
        std::vector<std::string> tokens;
        while (pos < input.length()) {
            char c = input[pos];

            if (std::isspace(c)) {
                pos++;
                continue;
            }

            if (c == '(' || c == ')') {
                tokens.push_back(std::string(1, c));
                pos++;
                continue;
            }

            if (c == '"') {
                std::string str;
                pos++; // пропускаем открывающую кавычку
                while (pos < input.length() && input[pos] != '"') {
                    if (input[pos] == '\\') {
                        pos++;
                        if (pos < input.length()) {
                            str += input[pos];
                        }
                    }
                    else {
                        str += input[pos];
                    }
                    pos++;
                }
                pos++; // пропускаем закрывающую кавычку
                tokens.push_back("\"" + str + "\"");
                continue;
            }

            // Число или символ
            std::string token;
            while (pos < input.length() && !std::isspace(input[pos]) &&
                input[pos] != '(' && input[pos] != ')') {
                token += input[pos];
                pos++;
            }
            tokens.push_back(token);
        }
        return tokens;
    }
};

class Parser {
    std::vector<std::string> tokens;
    size_t pos = 0;

    ObjectPtr parseAtom(const std::string& token) {
        // Число
        if (std::all_of(token.begin(), token.end(), ::isdigit) ||
            (token[0] == '-' && token.size() > 1 &&
                std::all_of(token.begin() + 1, token.end(), ::isdigit))) {
            return std::make_shared<Object>(Integer{ std::stoll(token) });
        }

        // Булево
        if (token == "#t") return std::make_shared<Object>(Boolean{ true });
        if (token == "#f") return std::make_shared<Object>(Boolean{ false });

        // Символ
        return std::make_shared<Object>(Symbol{ token });
    }

public:
    Parser(const std::vector<std::string>& tokens) : tokens(tokens) {}

    ObjectPtr parse() {
        if (pos >= tokens.size()) {
            return std::make_shared<Object>(Nil{});
        }

        std::string token = tokens[pos++];

        if (token == "(") {
            ObjectList list;
            while (pos < tokens.size() && tokens[pos] != ")") {
                list.push_back(parse());
            }
            if (pos < tokens.size()) pos++; // пропускаем ')'
            return std::make_shared<Object>(list);
        }
        else if (token == ")") {
            throw std::runtime_error("Unexpected ')'");
        }
        else {
            return parseAtom(token);
        }
    }
};

// ========== Вывод ==========
std::string toString(ObjectPtr obj) {
    if (obj->isNil()) return "()";
    if (obj->isInteger()) return std::to_string(obj->asInteger().value);
    if (obj->isBoolean()) return obj->asBoolean().value ? "#t" : "#f";
    if (obj->isString()) return "\"" + obj->asString().value + "\"";
    if (obj->isSymbol()) return obj->asSymbol().name;
    if (obj->isFunction()) return "<function>";
    if (obj->isList()) {
        auto& list = obj->asList();
        if (list.empty()) return "()";
        std::string result = "(";
        for (size_t i = 0; i < list.size(); i++) {
            if (i > 0) result += " ";
            result += toString(list[i]);
        }
        result += ")";
        return result;
    }
    return "unknown";
}

// ========== Оценщик (forward declaration) ==========
ObjectPtr eval(ObjectPtr expr, std::shared_ptr<Environment> env);

// ========== Примитивные функции ==========
ObjectPtr applyPrimitive(const std::string& name, const ObjectList& args,
    std::shared_ptr<Environment> env) {
    if (name == "+") {
        long long sum = 0;
        for (auto& arg : args) {
            ObjectPtr evaled = eval(arg, env);
            if (!evaled->isInteger()) throw std::runtime_error("+ requires numbers");
            sum += evaled->asInteger().value;
        }
        return std::make_shared<Object>(Integer{ sum });
    }

    if (name == "-") {
        if (args.empty()) throw std::runtime_error("- requires at least one argument");
        ObjectPtr first = eval(args[0], env);
        if (!first->isInteger()) throw std::runtime_error("- requires numbers");
        long long result = first->asInteger().value;
        for (size_t i = 1; i < args.size(); i++) {
            ObjectPtr arg = eval(args[i], env);
            if (!arg->isInteger()) throw std::runtime_error("- requires numbers");
            result -= arg->asInteger().value;
        }
        return std::make_shared<Object>(Integer{ result });
    }

    if (name == "*") {
        long long product = 1;
        for (auto& arg : args) {
            ObjectPtr evaled = eval(arg, env);
            if (!evaled->isInteger()) throw std::runtime_error("* requires numbers");
            product *= evaled->asInteger().value;
        }
        return std::make_shared<Object>(Integer{ product });
    }

    if (name == "/") {
        if (args.empty()) throw std::runtime_error("/ requires at least one argument");
        ObjectPtr first = eval(args[0], env);
        if (!first->isInteger()) throw std::runtime_error("/ requires numbers");
        long long result = first->asInteger().value;
        for (size_t i = 1; i < args.size(); i++) {
            ObjectPtr arg = eval(args[i], env);
            if (!arg->isInteger()) throw std::runtime_error("/ requires numbers");
            if (arg->asInteger().value == 0) throw std::runtime_error("division by zero");
            result /= arg->asInteger().value;
        }
        return std::make_shared<Object>(Integer{ result });
    }

    if (name == "=") {
        if (args.size() != 2) throw std::runtime_error("= requires exactly 2 arguments");
        ObjectPtr a = eval(args[0], env);
        ObjectPtr b = eval(args[1], env);
        if (!a->isInteger() || !b->isInteger()) throw std::runtime_error("= requires numbers");
        return std::make_shared<Object>(Boolean{ a->asInteger().value == b->asInteger().value });
    }

    if (name == "<") {
        if (args.size() != 2) throw std::runtime_error("< requires exactly 2 arguments");
        ObjectPtr a = eval(args[0], env);
        ObjectPtr b = eval(args[1], env);
        if (!a->isInteger() || !b->isInteger()) throw std::runtime_error("< requires numbers");
        return std::make_shared<Object>(Boolean{ a->asInteger().value < b->asInteger().value });
    }

    if (name == "cons") {
        if (args.size() != 2) throw std::runtime_error("cons requires exactly 2 arguments");
        ObjectPtr car = eval(args[0], env);
        ObjectPtr cdr = eval(args[1], env);

        ObjectList newList;
        newList.push_back(car);

        if (cdr->isList()) {
            auto& tail = cdr->asList();
            newList.insert(newList.end(), tail.begin(), tail.end());
        }
        else if (!cdr->isNil()) {
            // Improper list
            newList.push_back(cdr);
        }
        return std::make_shared<Object>(newList);
    }

    if (name == "car") {
        if (args.size() != 1) throw std::runtime_error("car requires exactly 1 argument");
        ObjectPtr arg = eval(args[0], env);
        if (!arg->isList() || arg->asList().empty()) {
            throw std::runtime_error("car: argument must be a non-empty list");
        }
        return arg->asList()[0];
    }

    if (name == "cdr") {
        if (args.size() != 1) throw std::runtime_error("cdr requires exactly 1 argument");
        ObjectPtr arg = eval(args[0], env);
        if (!arg->isList() || arg->asList().empty()) {
            throw std::runtime_error("cdr: argument must be a non-empty list");
        }
        auto& list = arg->asList();
        if (list.size() == 1) {
            return std::make_shared<Object>(Nil{});
        }
        ObjectList tail(list.begin() + 1, list.end());
        return std::make_shared<Object>(tail);
    }

    if (name == "list") {
        ObjectList list;
        for (auto& arg : args) {
            list.push_back(eval(arg, env));
        }
        return std::make_shared<Object>(list);
    }

    if (name == "null?") {
        if (args.size() != 1) throw std::runtime_error("null? requires exactly 1 argument");
        ObjectPtr arg = eval(args[0], env);
        return std::make_shared<Object>(Boolean{ arg->isNil() });
    }

    if (name == "display") {
        for (auto& arg : args) {
            std::cout << toString(eval(arg, env));
        }
        return std::make_shared<Object>(Nil{});
    }

    if (name == "newline") {
        std::cout << std::endl;
        return std::make_shared<Object>(Nil{});
    }

    throw std::runtime_error("Unknown primitive: " + name);
}

// ========== eval реализация ==========
ObjectPtr eval(ObjectPtr expr, std::shared_ptr<Environment> env) {
    // Самовычисляющиеся выражения
    if (expr->isInteger() || expr->isBoolean() || expr->isString() || expr->isNil()) {
        return expr;
    }

    // Символ - поиск в окружении
    if (expr->isSymbol()) {
        return env->find(expr->asSymbol().name);
    }

    // Список - форма
    if (expr->isList()) {
        auto& list = expr->asList();
        if (list.empty()) return expr; // пустой список

        ObjectPtr first = list[0];

        // quote
        if (first->isSymbol() && first->asSymbol().name == "quote") {
            if (list.size() != 2) throw std::runtime_error("quote requires exactly 1 argument");
            return list[1];
        }

        // if
        if (first->isSymbol() && first->asSymbol().name == "if") {
            if (list.size() != 4) throw std::runtime_error("if requires 3 arguments");
            ObjectPtr condition = eval(list[1], env);
            if (condition->isBoolean() && condition->asBoolean().value) {
                return eval(list[2], env);
            }
            else {
                return eval(list[3], env);
            }
        }

        // define
        if (first->isSymbol() && first->asSymbol().name == "define") {
            if (list.size() != 3) throw std::runtime_error("define requires 2 arguments");
            if (!list[1]->isSymbol()) {
                throw std::runtime_error("define: first argument must be a symbol");
            }
            std::string name = list[1]->asSymbol().name;
            ObjectPtr value = eval(list[2], env);
            env->define(name, value);
            return std::make_shared<Object>(Nil{});
        }

        // lambda
        if (first->isSymbol() && first->asSymbol().name == "lambda") {
            if (list.size() != 3) throw std::runtime_error("lambda requires 2 arguments");
            if (!list[1]->isList()) {
                throw std::runtime_error("lambda: parameters must be a list");
            }

            std::vector<std::string> params;
            for (auto& param : list[1]->asList()) {
                if (!param->isSymbol()) {
                    throw std::runtime_error("lambda: parameters must be symbols");
                }
                params.push_back(param->asSymbol().name);
            }

            Function func;
            func.params = params;
            func.body = list[2];
            func.env = env;
            return std::make_shared<Object>(func);
        }

        // Функция (вызов)
        ObjectPtr func = eval(first, env);

        // Примитивная функция
        if (func->isSymbol()) {
            ObjectList args;
            for (size_t i = 1; i < list.size(); i++) {
                args.push_back(list[i]);
            }
            return applyPrimitive(func->asSymbol().name, args, env);
        }

        // Пользовательская функция
        if (func->isFunction()) {
            auto& f = func->asFunction();
            auto newEnv = std::make_shared<Environment>(f.env);

            // Связываем параметры с аргументами
            auto& argsList = list;
            if (argsList.size() - 1 != f.params.size()) {
                throw std::runtime_error("Wrong number of arguments");
            }

            for (size_t i = 0; i < f.params.size(); i++) {
                ObjectPtr argValue = eval(argsList[i + 1], env);
                newEnv->define(f.params[i], argValue);
            }

            return eval(f.body, newEnv);
        }

        throw std::runtime_error("Cannot evaluate expression");
    }

    throw std::runtime_error("Unknown expression type");
}

// ========== REPL ==========
void repl() {
    auto globalEnv = std::make_shared<Environment>();

    // Добавляем примитивы
    globalEnv->define("+", std::make_shared<Object>(Symbol{ "+" }));
    globalEnv->define("-", std::make_shared<Object>(Symbol{ "-" }));
    globalEnv->define("*", std::make_shared<Object>(Symbol{ "*" }));
    globalEnv->define("/", std::make_shared<Object>(Symbol{ "/" }));
    globalEnv->define("=", std::make_shared<Object>(Symbol{ "=" }));
    globalEnv->define("<", std::make_shared<Object>(Symbol{ "<" }));
    globalEnv->define("cons", std::make_shared<Object>(Symbol{ "cons" }));
    globalEnv->define("car", std::make_shared<Object>(Symbol{ "car" }));
    globalEnv->define("cdr", std::make_shared<Object>(Symbol{ "cdr" }));
    globalEnv->define("list", std::make_shared<Object>(Symbol{ "list" }));
    globalEnv->define("null?", std::make_shared<Object>(Symbol{ "null?" }));
    globalEnv->define("display", std::make_shared<Object>(Symbol{ "display" }));
    globalEnv->define("newline", std::make_shared<Object>(Symbol{ "newline" }));

    std::string input;
    while (true) {
        std::cout << "minilisp> ";
        std::getline(std::cin, input);

        if (input == "exit" || input == "quit") break;
        if (input.empty()) continue;

        try {
            Tokenizer tokenizer(input);
            auto tokens = tokenizer.tokenize();
            Parser parser(tokens);
            auto expr = parser.parse();
            auto result = eval(expr, globalEnv);
            std::cout << toString(result) << std::endl;
        }
        catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
}

int main() {
    std::cout << "MiniLisp Interpreter v1.0" << std::endl;
    std::cout << "Type 'exit' to quit" << std::endl;
    repl();
    return 0;
}