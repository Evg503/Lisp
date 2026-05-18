// В начале файла добавьте
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>
#include <algorithm>
#include <sstream>
#include <optional>
#include <cctype>
#include <fstream>
#include <filesystem>

// ========== Безопасные функции для работы с символами ==========
inline bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

inline bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// ========== Типы данных (без изменений) ==========
struct Symbol {
    std::string name;
    bool operator==(const Symbol& other) const { return name == other.name; }
};

// Forward declarations
struct Object;
using ObjectPtr = std::shared_ptr<Object>;
using ObjectList = std::vector<ObjectPtr>;

class Environment;

struct Nil {};
struct Integer { long long value; };
struct Boolean { bool value; };
struct String { std::string value; };

struct Function {
    std::vector<std::string> params;
    ObjectPtr body;
    std::shared_ptr<Environment> env;
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

// ========== Исправленный Tokenizer с безопасными функциями ==========
class Tokenizer {
    std::string input;
    size_t pos = 0;

    char peek() const {
        if (pos >= input.length()) return '\0';
        return input[pos];
    }

    char advance() {
        if (pos >= input.length()) return '\0';
        return input[pos++];
    }

    // Безопасная проверка на пробел
    bool isSpace(char c) const {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

public:
    Tokenizer(const std::string& src) : input(src) {}

    std::vector<std::string> tokenize() {
        std::vector<std::string> tokens;

        while (pos < input.length()) {
            char c = peek();

            // Пропускаем пробелы
            if (isSpace(c)) {
                advance();
                continue;
            }

            // Скобки
            if (c == '(' || c == ')') {
                tokens.push_back(std::string(1, c));
                advance();
                continue;
            }

            // Строки
            if (c == '"') {
                std::string str;
                advance(); // пропускаем открывающую кавычку

                while (pos < input.length() && peek() != '"') {
                    if (peek() == '\\') {
                        advance(); // пропускаем backslash
                        if (pos < input.length()) {
                            char escaped = advance();
                            switch (escaped) {
                            case 'n': str += '\n'; break;
                            case 't': str += '\t'; break;
                            case 'r': str += '\r'; break;
                            case '\\': str += '\\'; break;
                            case '"': str += '"'; break;
                            default: str += escaped; break;
                            }
                        }
                    }
                    else {
                        str += advance();
                    }
                }

                if (pos >= input.length()) {
                    throw std::runtime_error("Unclosed string literal");
                }

                advance(); // пропускаем закрывающую кавычку
                tokens.push_back("\"" + str + "\"");
                continue;
            }

            // Числа, символы и булевы значения
            std::string token;
            while (pos < input.length() && !isSpace(peek()) &&
                peek() != '(' && peek() != ')' && peek() != '"') {
                token += advance();
            }

            // Проверяем на булевы значения
            if (token == "#t" || token == "#f") {
                tokens.push_back(token);
            }
            else {
                tokens.push_back(token);
            }
        }

        return tokens;
    }
};

// ========== Исправленный Parser ==========
class Parser {
    std::vector<std::string> tokens;
    size_t pos = 0;

    bool isNumber(const std::string& token) {
        if (token.empty()) return false;
        size_t start = (token[0] == '-') ? 1 : 0;
        if (start >= token.length()) return false;

        // Безопасная проверка цифр
        for (size_t i = start; i < token.length(); i++) {
            char c = token[i];
            if (c < '0' || c > '9') return false;
        }
        return true;
    }

    ObjectPtr parseAtom(const std::string& token) {
        // Число
        if (isNumber(token)) {
            return std::make_shared<Object>(Integer{ std::stoll(token) });
        }

        // Булево
        if (token == "#t") return std::make_shared<Object>(Boolean{ true });
        if (token == "#f") return std::make_shared<Object>(Boolean{ false });

        // Строка (убираем кавычки)
        if (token.length() >= 2 && token.front() == '"' && token.back() == '"') {
            std::string content = token.substr(1, token.length() - 2);
            return std::make_shared<Object>(String{ content });
        }

        // Символ
        return std::make_shared<Object>(Symbol{ token });
    }

public:
    Parser(const std::vector<std::string>& tokens) : tokens(tokens) {}

    bool hasMore() const {
        return pos < tokens.size();
    }

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
            if (pos >= tokens.size()) {
                throw std::runtime_error("Unclosed parenthesis");
            }
            pos++; // пропускаем ')'
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

// ========== Вывод (без изменений) ==========
std::string toString(ObjectPtr obj) {
    if (obj->isNil()) return "()";
    if (obj->isInteger()) return std::to_string(obj->asInteger().value);
    if (obj->isBoolean()) return obj->asBoolean().value ? "#t" : "#f";
    if (obj->isString()) {
        std::string result = "\"";
        for (char c : obj->asString().value) {
            switch (c) {
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            case '\r': result += "\\r"; break;
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            default: result += c; break;
            }
        }
        result += "\"";
        return result;
    }
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

std::string toDisplayString(ObjectPtr obj) {
    if (obj->isNil()) return "()";
    if (obj->isInteger()) return std::to_string(obj->asInteger().value);
    if (obj->isBoolean()) return obj->asBoolean().value ? "#t" : "#f";
    if (obj->isString()) {
        return obj->asString().value;
    }
    if (obj->isSymbol()) return obj->asSymbol().name;
    if (obj->isFunction()) return "<function>";
    if (obj->isList()) {
        auto& list = obj->asList();
        if (list.empty()) return "()";
        std::string result = "(";
        for (size_t i = 0; i < list.size(); i++) {
            if (i > 0) result += " ";
            result += toDisplayString(list[i]);
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
            ObjectPtr evaled = eval(arg, env);
            std::cout << toDisplayString(evaled);
        }
        return std::make_shared<Object>(Nil{});
    }

    if (name == "print") {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) std::cout << " ";
            ObjectPtr evaled = eval(args[i], env);
            std::cout << toDisplayString(evaled);
        }
        std::cout << std::endl;
        return std::make_shared<Object>(Nil{});
    }

    if (name == "write") {
        for (auto& arg : args) {
            std::cout << toString(eval(arg, env));
        }
        return std::make_shared<Object>(Nil{});
    }

    if (name == "newline") {
        std::cout << std::endl;
        return std::make_shared<Object>(Nil{});
    }

    if (name == "load") {
        if (args.size() != 1) throw std::runtime_error("load requires exactly 1 argument");
        ObjectPtr filename = eval(args[0], env);
        if (!filename->isString()) throw std::runtime_error("load: argument must be a string");

        // Здесь должна быть функция loadFile
        // loadFile(filename->asString().value, env);
        return std::make_shared<Object>(Nil{});
    }

    throw std::runtime_error("Unknown primitive: " + name);
}

// ========== eval реализация ==========
ObjectPtr eval(ObjectPtr expr, std::shared_ptr<Environment> env) {
    if (expr->isInteger() || expr->isBoolean() || expr->isString() || expr->isNil()) {
        return expr;
    }

    if (expr->isSymbol()) {
        return env->find(expr->asSymbol().name);
    }

    if (expr->isList()) {
        auto& list = expr->asList();
        if (list.empty()) return expr;

        ObjectPtr first = list[0];

        if (first->isSymbol() && first->asSymbol().name == "quote") {
            if (list.size() != 2) throw std::runtime_error("quote requires exactly 1 argument");
            return list[1];
        }

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

        ObjectPtr func = eval(first, env);

        if (func->isSymbol()) {
            ObjectList args;
            for (size_t i = 1; i < list.size(); i++) {
                args.push_back(list[i]);
            }
            return applyPrimitive(func->asSymbol().name, args, env);
        }

        if (func->isFunction()) {
            auto& f = func->asFunction();
            auto newEnv = std::make_shared<Environment>(f.env);

            if (list.size() - 1 != f.params.size()) {
                throw std::runtime_error("Wrong number of arguments");
            }

            for (size_t i = 0; i < f.params.size(); i++) {
                ObjectPtr argValue = eval(list[i + 1], env);
                newEnv->define(f.params[i], argValue);
            }

            return eval(f.body, newEnv);
        }

        throw std::runtime_error("Cannot evaluate expression");
    }

    throw std::runtime_error("Unknown expression type");
}

// ========== Функция для загрузки файла ==========
void loadFile(const std::string& filename, std::shared_ptr<Environment> env) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    if (content.empty()) return;

    Tokenizer tokenizer(content);
    auto tokens = tokenizer.tokenize();
    Parser parser(tokens);

    std::vector<ObjectPtr> expressions;
    while (parser.hasMore()) {
        expressions.push_back(parser.parse());
    }

    for (auto& expr : expressions) {
        eval(expr, env);
    }
}

// ========== Кроссплатформенная функция для домашней директории ==========
std::string getHomeDirectory() {
    const char* home = std::getenv("HOME");
    if (home) return std::string(home);

    // Windows fallback
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) return std::string(userprofile);

    return "";
}

// ========== Поиск init-файла ==========
std::string findInitFile() {
    // Проверяем текущую директорию
    if (std::filesystem::exists(".minilisp.scm")) {
        return ".minilisp.scm";
    }
    if (std::filesystem::exists(".minilisp")) {
        return ".minilisp";
    }

    // Проверяем домашнюю директорию
    std::string home = getHomeDirectory();
    if (!home.empty()) {
        std::string initPath = home + "/.minilisp.scm";
        if (std::filesystem::exists(initPath)) {
            return initPath;
        }
        initPath = home + "/.minilisp";
        if (std::filesystem::exists(initPath)) {
            return initPath;
        }
    }

    return "";
}

// ========== REPL ==========
void repl(std::shared_ptr<Environment> env) {
    std::string input;

    std::cout << "MiniLisp Interpreter v1.0" << std::endl;
    std::cout << "Type 'exit' or 'quit' to quit" << std::endl;

    while (true) {
        std::cout << "minilisp> ";
        std::getline(std::cin, input);

        if (input == "exit" || input == "quit") break;
        if (input.empty()) continue;

        try {
            Tokenizer tokenizer(input);
            auto tokens = tokenizer.tokenize();
            Parser parser(tokens);

            std::vector<ObjectPtr> expressions;
            while (parser.hasMore()) {
                expressions.push_back(parser.parse());
            }

            if (expressions.empty()) continue;

            ObjectPtr lastResult = nullptr;
            for (auto& expr : expressions) {
                lastResult = eval(expr, env);
            }

            if (lastResult && !expressions.empty()) {
                ObjectPtr lastExpr = expressions.back();
                bool isSilent = false;

                if (lastExpr->isList()) {
                    auto& list = lastExpr->asList();
                    if (!list.empty() && list[0]->isSymbol()) {
                        std::string funcName = list[0]->asSymbol().name;
                        if (funcName == "display" || funcName == "print" ||
                            funcName == "newline" || funcName == "define" ||
                            funcName == "load") {
                            isSilent = true;
                        }
                    }
                }

                if (!isSilent && !lastResult->isNil()) {
                    std::cout << toString(lastResult) << std::endl;
                }
            }
        }
        catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
}

// ========== main ==========
int main() {
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
    globalEnv->define("print", std::make_shared<Object>(Symbol{ "print" }));
    globalEnv->define("write", std::make_shared<Object>(Symbol{ "write" }));
    globalEnv->define("newline", std::make_shared<Object>(Symbol{ "newline" }));
    globalEnv->define("load", std::make_shared<Object>(Symbol{ "load" }));

    // Загружаем init-файл
    std::string initFile = findInitFile();
    if (!initFile.empty()) {
        try {
            loadFile(initFile, globalEnv);
        }
        catch (const std::exception& e) {
            std::cerr << "Warning: Failed to load init file: " << e.what() << std::endl;
        }
    }

    // Запускаем REPL
    repl(globalEnv);

    return 0;
}