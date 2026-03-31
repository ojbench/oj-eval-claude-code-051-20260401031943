#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cctype>
#include <algorithm>

using namespace std;

struct Token {
    enum Type { LABEL, OPCODE, NUMBER, ID, LPAREN, RPAREN, PLUS, MINUS, SEMICOLON, COLON, QUESTION, DOT } type;
    string value;
    int line;
};

class Lexer {
public:
    vector<Token> tokenize(const string& input) {
        vector<Token> tokens;
        int line = 1;
        size_t i = 0;

        while (i < input.size()) {
            // Skip whitespace
            if (isspace(input[i])) {
                if (input[i] == '\n') line++;
                i++;
                continue;
            }

            // Skip comments
            if (i + 1 < input.size() && input[i] == '/' && input[i+1] == '/') {
                while (i < input.size() && input[i] != '\n') i++;
                continue;
            }

            // Numbers (including negative)
            if (isdigit(input[i]) || (input[i] == '-' && i + 1 < input.size() && isdigit(input[i+1]))) {
                size_t start = i;
                if (input[i] == '-') i++;
                while (i < input.size() && isdigit(input[i])) i++;
                tokens.push_back({Token::NUMBER, input.substr(start, i - start), line});
                continue;
            }

            // Identifiers and opcodes
            if (isalpha(input[i]) || input[i] == '_') {
                size_t start = i;
                while (i < input.size() && (isalnum(input[i]) || input[i] == '_')) i++;
                tokens.push_back({Token::ID, input.substr(start, i - start), line});
                continue;
            }

            // Single character tokens
            switch (input[i]) {
                case '(': tokens.push_back({Token::LPAREN, "(", line}); break;
                case ')': tokens.push_back({Token::RPAREN, ")", line}); break;
                case '+': tokens.push_back({Token::PLUS, "+", line}); break;
                case '-': tokens.push_back({Token::MINUS, "-", line}); break;
                case ';': tokens.push_back({Token::SEMICOLON, ";", line}); break;
                case ':': tokens.push_back({Token::COLON, ":", line}); break;
                case '?': tokens.push_back({Token::QUESTION, "?", line}); break;
                case '.': tokens.push_back({Token::DOT, ".", line}); break;
            }
            i++;
        }

        return tokens;
    }
};

struct Expression {
    size_t startPos;
    size_t endPos;
    int targetAddress;
};

class Parser {
private:
    vector<Token> tokens;
    size_t pos;
    map<string, int> labels;
    vector<int> memory;
    int currentAddress;
    int evalAddress;  // The address for which we're evaluating an expression

    Token peek() {
        if (pos < tokens.size()) return tokens[pos];
        return {Token::SEMICOLON, "", -1};
    }

    Token consume() {
        return tokens[pos++];
    }

    bool match(Token::Type type) {
        return peek().type == type;
    }

    void skipExpression() {
        skipTerm();
        while (match(Token::PLUS) || match(Token::MINUS)) {
            consume();
            skipTerm();
        }
    }

    void skipTerm() {
        if (match(Token::MINUS)) {
            consume();
            skipTerm();
        } else if (match(Token::LPAREN)) {
            consume();
            skipExpression();
            if (match(Token::RPAREN)) consume();
        } else if (match(Token::NUMBER) || match(Token::QUESTION) || match(Token::ID)) {
            consume();
        }
    }

    int parseExpression() {
        int result = parseTerm();
        while (match(Token::PLUS) || match(Token::MINUS)) {
            bool isPlus = match(Token::PLUS);
            consume();
            int term = parseTerm();
            if (isPlus) result += term;
            else result -= term;
        }
        return result;
    }

    int parseTerm() {
        if (match(Token::MINUS)) {
            consume();
            return -parseTerm();
        } else if (match(Token::LPAREN)) {
            consume();
            int result = parseExpression();
            if (match(Token::RPAREN)) consume();
            return result;
        } else if (match(Token::NUMBER)) {
            return stoi(consume().value);
        } else if (match(Token::QUESTION)) {
            consume();
            // ? means the address where this expression's value will be stored, + 1
            return evalAddress + 1;
        } else if (match(Token::ID)) {
            string id = consume().value;
            if (labels.count(id)) {
                return labels[id];
            }
            return 0; // Unknown label
        }
        return 0;
    }

    void firstPass() {
        while (pos < tokens.size()) {
            // Check for labels
            while (match(Token::ID) && pos + 1 < tokens.size() && tokens[pos + 1].type == Token::COLON) {
                string label = consume().value;
                consume(); // consume ':'
                labels[label] = currentAddress;
            }

            if (match(Token::SEMICOLON)) {
                consume();
                continue;
            }

            // Check opcode
            if (match(Token::DOT)) {
                consume(); // '.'
                // Count items until semicolon
                while (!match(Token::SEMICOLON) && pos < tokens.size()) {
                    // Skip label if present
                    if (match(Token::ID) && pos + 1 < tokens.size() && tokens[pos + 1].type == Token::COLON) {
                        string label = consume().value;
                        consume(); // ':'
                        labels[label] = currentAddress;
                    }

                    // Parse expression (but don't evaluate yet)
                    skipExpression();
                    currentAddress++;
                }
            } else if (match(Token::ID)) {
                Token opcode = consume();
                currentAddress++; // opcode takes 1 slot

                int paramCount = 0;
                while (!match(Token::SEMICOLON) && pos < tokens.size()) {
                    if (match(Token::ID) && pos + 1 < tokens.size() && tokens[pos + 1].type == Token::COLON) {
                        string label = consume().value;
                        consume();
                        labels[label] = currentAddress;
                    }
                    skipExpression();
                    currentAddress++;
                    paramCount++;
                }

                // Auto-expand msubleq and rsubleq
                if ((opcode.value == "msubleq" || opcode.value == "rsubleq") && paramCount < 3) {
                    currentAddress += (3 - paramCount);
                }
            }

            if (match(Token::SEMICOLON)) consume();
        }
    }

    void secondPass() {
        while (pos < tokens.size()) {
            // Skip labels
            while (match(Token::ID) && pos + 1 < tokens.size() && tokens[pos + 1].type == Token::COLON) {
                consume(); // label
                consume(); // ':'
            }

            if (match(Token::SEMICOLON)) {
                consume();
                continue;
            }

            // Handle '.' directive
            if (match(Token::DOT)) {
                consume();
                while (!match(Token::SEMICOLON) && pos < tokens.size()) {
                    // Skip labels
                    if (match(Token::ID) && pos + 1 < tokens.size() && tokens[pos + 1].type == Token::COLON) {
                        consume();
                        consume();
                    }
                    evalAddress = currentAddress;
                    int value = parseExpression();
                    memory.push_back(value);
                    currentAddress++;
                }
            } else if (match(Token::ID)) {
                Token opcode = consume();
                int opcodeValue = -1;

                if (opcode.value == "msubleq") opcodeValue = 0;
                else if (opcode.value == "rsubleq") opcodeValue = 1;
                else if (opcode.value == "ldorst") opcodeValue = 2;

                memory.push_back(opcodeValue);
                currentAddress++;

                vector<int> params;
                int numExplicitParams = 0;

                // First, determine how many parameters we'll have in total
                size_t savedPos = pos;
                while (!match(Token::SEMICOLON) && pos < tokens.size()) {
                    if (match(Token::ID) && pos + 1 < tokens.size() && tokens[pos + 1].type == Token::COLON) {
                        consume();
                        consume();
                    }
                    skipExpression();
                    numExplicitParams++;
                }

                int totalParams = numExplicitParams;
                if ((opcode.value == "msubleq" || opcode.value == "rsubleq") && totalParams < 3) {
                    totalParams = 3;
                }

                // Now parse parameters with correct addresses
                pos = savedPos;
                for (int i = 0; i < numExplicitParams; i++) {
                    if (match(Token::ID) && pos + 1 < tokens.size() && tokens[pos + 1].type == Token::COLON) {
                        consume();
                        consume();
                    }
                    evalAddress = currentAddress;
                    int value = parseExpression();
                    params.push_back(value);
                    memory.push_back(value);
                    currentAddress++;
                }

                // Auto-expand msubleq and rsubleq
                if ((opcode.value == "msubleq" || opcode.value == "rsubleq") && params.size() < 3) {
                    while (params.size() < 2) {
                        int val = params.empty() ? 0 : params[0];
                        memory.push_back(val);
                        currentAddress++;
                        params.push_back(val);
                    }
                    if (params.size() < 3) {
                        // ? for auto-expanded third parameter
                        evalAddress = currentAddress;
                        memory.push_back(evalAddress + 1);
                        currentAddress++;
                    }
                }
            }

            if (match(Token::SEMICOLON)) consume();
        }
    }

public:
    vector<int> parse(const vector<Token>& toks) {
        tokens = toks;
        pos = 0;
        currentAddress = 0;

        // First pass: collect labels
        firstPass();

        // Second pass: generate code
        pos = 0;
        currentAddress = 0;
        memory.clear();
        secondPass();

        return memory;
    }
};

int main() {
    string line, input;
    while (getline(cin, line)) {
        input += line + "\n";
    }

    Lexer lexer;
    vector<Token> tokens = lexer.tokenize(input);

    Parser parser;
    vector<int> memory = parser.parse(tokens);

    // Output in the required format
    for (size_t i = 0; i < memory.size(); i++) {
        if (i > 0 && i % 4 == 0) cout << "\n";
        else if (i > 0) cout << " ";
        cout << memory[i];
    }
    cout << "\n";

    return 0;
}
