
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct Line {
	int number{};
	int indent{};
	std::string text;
};

struct Target {
	std::optional<std::string> registry;
	std::string key;
};

enum class ValueKind { Literal, Builtin };

struct ValueExpr {
	ValueKind kind{};
	std::string text; // literal value or builtin name
};

enum class StatementType { Registry, If, Store, Replace, Note, Secure };

struct Statement;

struct IfStmt {
	bool isMissing{}; // true => missing, false => present
	Target target;
	std::vector<Statement> body;
};

struct Statement {
	StatementType type{};
	int line{};
	std::string registryName; // for registry
	Target target;            // for if/store/replace
	ValueExpr value;          // for store/replace
	IfStmt conditional;       // for if
	std::string note;         // for note
};

struct VaultBlock {
	bool optional{};
	std::string name;
	int line{};
	std::vector<Statement> body;
};

// Utility helpers
std::string trim(const std::string &s) {
	auto start = s.find_first_not_of(' ');
	if (start == std::string::npos) return "";
	auto end = s.find_last_not_of(' ');
	return s.substr(start, end - start + 1);
}

bool starts_with(const std::string &s, const std::string &prefix) {
	return s.rfind(prefix, 0) == 0;
}

std::string read_file(const std::string &path, std::vector<Line> &lines) {
	std::ifstream in(path);
	if (!in) throw std::runtime_error("Unable to open file: " + path);
	std::string content;
	std::string line;
	int number = 1;
	while (std::getline(in, line)) {
		int indent = 0;
		for (char c : line) {
			if (c == ' ') indent++; else break;
		}
		if (line.find('\t') != std::string::npos) {
			throw std::runtime_error("Tabs are not allowed (line " + std::to_string(number) + ")");
		}
		lines.push_back({number, indent, line.substr(indent)});
		content += line;
		content += '\n';
		number++;
	}
	return content;
}

std::string expect_quoted(const std::string &text, int line) {
	auto trimmed = trim(text);
	if (trimmed.size() < 2 || trimmed.front() != '"' || trimmed.back() != '"') {
		throw std::runtime_error("Expected quoted string on line " + std::to_string(line));
	}
	return trimmed.substr(1, trimmed.size() - 2);
}

Target parse_target(const std::string &text, int line) {
	auto expr = trim(text);
	auto arrow = expr.find("->");
	if (arrow == std::string::npos) {
		throw std::runtime_error("Expected '->' in target on line " + std::to_string(line));
	}
	std::string left = trim(expr.substr(0, arrow));
	std::string right = trim(expr.substr(arrow + 2));
	Target t;
	if (!left.empty() && left != "->") t.registry = left;
	t.key = expect_quoted(right, line);
	return t;
}

ValueExpr parse_value_expr(const std::string &text, int line) {
	auto trimmed = trim(text);
	if (trimmed.empty()) throw std::runtime_error("Missing value on line " + std::to_string(line));
	if (trimmed.front() == '"') {
		return {ValueKind::Literal, expect_quoted(trimmed, line)};
	}
	// builtin form: ident()
	auto open = trimmed.find('(');
	auto close = trimmed.find(')');
	if (open != std::string::npos && close == trimmed.size() - 1 && open == close - 1) {
		std::string name = trimmed.substr(0, open);
		if (name.empty()) throw std::runtime_error("Bad builtin on line " + std::to_string(line));
		return {ValueKind::Builtin, name};
	}
	throw std::runtime_error("Unrecognized value expression on line " + std::to_string(line));
}

class Parser {
  public:
	explicit Parser(std::vector<Line> lines) : lines_(std::move(lines)) {}

	std::vector<VaultBlock> parse() {
		std::vector<VaultBlock> program;
		while (pos_ < lines_.size()) {
			auto &line = lines_[pos_];
			if (trim(line.text).empty()) { pos_++; continue; }
			if (line.indent != 0) {
				throw std::runtime_error("Top-level statements must start at indent 0 (line " + std::to_string(line.number) + ")");
			}
			program.push_back(parse_vault());
		}
		return program;
	}

  private:
	VaultBlock parse_vault() {
		auto &line = lines_[pos_];
		auto body = trim(line.text);
		bool optional = false;
		std::string name;
		if (starts_with(body, "vault? ")) {
			optional = true;
			name = trim(body.substr(7));
		} else if (starts_with(body, "vault ")) {
			name = trim(body.substr(6));
		} else {
			throw std::runtime_error("Expected 'vault' declaration on line " + std::to_string(line.number));
		}
		if (name.empty()) throw std::runtime_error("Vault name missing on line " + std::to_string(line.number));
		pos_++;
		auto bodyStatements = parse_block(line.indent + 2);
		bool hasSecure = !bodyStatements.empty() && bodyStatements.back().type == StatementType::Secure;
		if (!hasSecure) {
			throw std::runtime_error("Vault '" + name + "' missing terminating 'secure' (line " + std::to_string(line.number) + ")");
		}
		return VaultBlock{optional, name, line.number, std::move(bodyStatements)};
	}

	std::vector<Statement> parse_block(int indent) {
		std::vector<Statement> stmts;
		while (pos_ < lines_.size()) {
			auto &line = lines_[pos_];
			if (trim(line.text).empty()) { pos_++; continue; }
			if (line.indent < indent) break;
			if (line.indent != indent) {
				throw std::runtime_error("Unexpected indent on line " + std::to_string(line.number));
			}
			auto parsed = parse_statement();
			stmts.push_back(std::move(parsed));
		}
		return stmts;
	}

	Statement parse_statement() {
		auto &line = lines_[pos_];
		auto text = trim(line.text);

		if (starts_with(text, "registry ")) {
			Statement s;
			s.type = StatementType::Registry;
			s.line = line.number;
			s.registryName = trim(text.substr(9));
			if (s.registryName.empty()) throw std::runtime_error("Registry name missing on line " + std::to_string(line.number));
			pos_++;
			return s;
		}

		if (starts_with(text, "if " )) {
			Statement s;
			s.type = StatementType::If;
			s.line = line.number;
			auto rest = trim(text.substr(3));
			if (starts_with(rest, "missing ")) {
				s.conditional.isMissing = true;
				rest = trim(rest.substr(8));
			} else if (starts_with(rest, "present ")) {
				s.conditional.isMissing = false;
				rest = trim(rest.substr(8));
			} else {
				throw std::runtime_error("Expected 'missing' or 'present' on line " + std::to_string(line.number));
			}
			s.conditional.target = parse_target(rest, line.number);
			pos_++;
			s.conditional.body = parse_block(line.indent + 2);
			return s;
		}

		if (starts_with(text, "store ")) {
			Statement s;
			s.type = StatementType::Store;
			s.line = line.number;
			auto rest = trim(text.substr(6));
			auto eq = rest.find('=');
			if (eq == std::string::npos) throw std::runtime_error("Missing '=' on line " + std::to_string(line.number));
			auto targetText = trim(rest.substr(0, eq));
			auto valueText = trim(rest.substr(eq + 1));
			s.target = parse_target(targetText, line.number);
			s.value = parse_value_expr(valueText, line.number);
			pos_++;
			return s;
		}

		if (starts_with(text, "replace ")) {
			Statement s;
			s.type = StatementType::Replace;
			s.line = line.number;
			auto rest = trim(text.substr(8));
			auto eq = rest.find('=');
			if (eq == std::string::npos) throw std::runtime_error("Missing '=' on line " + std::to_string(line.number));
			s.target = parse_target(trim(rest.substr(0, eq)), line.number);
			s.value = parse_value_expr(trim(rest.substr(eq + 1)), line.number);
			pos_++;
			return s;
		}

		if (starts_with(text, "note ")) {
			Statement s;
			s.type = StatementType::Note;
			s.line = line.number;
			s.note = expect_quoted(text.substr(5), line.number);
			pos_++;
			return s;
		}

		if (text == "secure") {
			Statement s;
			s.type = StatementType::Secure;
			s.line = line.number;
			pos_++;
			return s;
		}

		throw std::runtime_error("Unknown statement on line " + std::to_string(line.number) + ": " + text);
	}

	std::vector<Line> lines_;
	std::size_t pos_{};
};

// Interpreter
struct RegistryState {
	std::unordered_map<std::string, std::string> entries; // stored as opaque
};

struct VaultState {
	bool sealed{};
	bool exists{true};
	std::unordered_map<std::string, RegistryState> registries;
};

class Interpreter {
  public:
	Interpreter(bool verbose, bool materializeOptional)
		: verbose_(verbose), materializeOptional_(materializeOptional) {}

	void run(const std::vector<VaultBlock> &program) {
		for (const auto &vault : program) {
			evaluate_vault(vault);
		}
		if (verbose_) {
			print_state();
		}
	}

  private:
	std::unordered_map<std::string, VaultState> state_;
	bool verbose_{};
	bool materializeOptional_{};
	std::optional<std::string> currentRegistry_;
	std::string currentVault_;

	void log(const std::string &msg) const {
		std::cout << msg << "\n";
	}

	std::string make_ciphertext(const std::string &plain) {
		return "ENC[" + plain + "]";
	}

	std::string generate_random() {
		static std::mt19937 rng{std::random_device{}()};
		std::uniform_int_distribution<int> dist(0, 15);
		std::string out;
		out.reserve(32);
		for (int i = 0; i < 32; ++i) {
			int v = dist(rng);
			out.push_back("0123456789abcdef"[v]);
		}
		return out;
	}

	std::string builtin_value(const ValueExpr &v) {
		if (v.kind == ValueKind::Literal) return v.text;
		if (v.text == "generate") return generate_random();
		if (v.text == "now") {
			auto now = std::chrono::system_clock::now();
			auto t = std::chrono::system_clock::to_time_t(now);
			std::tm tm{};
#ifdef _WIN32
			localtime_s(&tm, &t);
#else
			localtime_r(&t, &tm);
#endif
			std::ostringstream oss;
			oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
			return oss.str();
		}
		throw std::runtime_error("Unknown builtin: " + v.text);
	}

	RegistryState &require_registry(const Target &t, int line) {
		auto &vault = state_.at(currentVault_);
		std::string regName;
		if (t.registry) {
			regName = *t.registry;
		} else if (currentRegistry_) {
			regName = *currentRegistry_;
		} else {
			throw std::runtime_error("No active registry for target on line " + std::to_string(line));
		}
		return vault.registries[regName];
	}

	void evaluate_vault(const VaultBlock &vault) {
		currentRegistry_.reset();
		currentVault_.clear();
		auto found = state_.find(vault.name);
		bool exists = found != state_.end();
		if (vault.optional && !exists && !materializeOptional_) {
			log("[skip] optional vault '" + vault.name + "' not present");
			return;
		}

		VaultState &vstate = state_[vault.name];
		vstate.exists = true;
		currentVault_ = vault.name;
		log(std::string("[vault] ") + (vault.optional ? "optional " : "required ") + vault.name);

		for (const auto &stmt : vault.body) {
			execute_statement(stmt, vstate);
		}
	}

	bool is_present(const Target &t, int line) {
		auto &reg = require_registry(t, line);
		return reg.entries.find(t.key) != reg.entries.end();
	}

	void execute_statement(const Statement &s, VaultState &vstate) {
		switch (s.type) {
		case StatementType::Registry:
			if (vstate.sealed) throw std::runtime_error("Cannot select registry after secure (line " + std::to_string(s.line) + ")");
			currentRegistry_ = s.registryName;
			log("  [registry] " + s.registryName);
			break;
		case StatementType::If: {
			bool present = is_present(s.conditional.target, s.line);
			bool cond = s.conditional.isMissing ? !present : present;
			log(std::string("  [if] ") + (s.conditional.isMissing ? "missing " : "present ") + "-> '" + s.conditional.target.key + "' => " + (cond ? "true" : "false"));
			if (cond) {
				for (const auto &inner : s.conditional.body) execute_statement(inner, vstate);
			}
			break;
		}
		case StatementType::Store: {
			if (vstate.sealed) throw std::runtime_error("Cannot store after secure (line " + std::to_string(s.line) + ")");
			auto &reg = require_registry(s.target, s.line);
			if (reg.entries.count(s.target.key)) {
				throw std::runtime_error("store would overwrite existing key on line " + std::to_string(s.line));
			}
			auto val = builtin_value(s.value);
			reg.entries[s.target.key] = make_ciphertext(val);
			log("  [store] " + s.target.key);
			break;
		}
		case StatementType::Replace: {
			if (vstate.sealed) throw std::runtime_error("Cannot replace after secure (line " + std::to_string(s.line) + ")");
			auto &reg = require_registry(s.target, s.line);
			auto val = builtin_value(s.value);
			reg.entries[s.target.key] = make_ciphertext(val);
			log("  [replace] " + s.target.key);
			break;
		}
		case StatementType::Note:
			log("  [note] " + s.note);
			break;
		case StatementType::Secure:
			vstate.sealed = true;
			log("  [secure] vault sealed");
			break;
		}
	}

	void print_state() {
		std::cout << "\n=== Vault State ===\n";
		for (const auto &pair : state_) {
			std::cout << "vault " << pair.first << "\n";
			for (const auto &regPair : pair.second.registries) {
				std::cout << "  registry " << regPair.first << "\n";
				for (const auto &entry : regPair.second.entries) {
					std::cout << "    " << entry.first << " : " << entry.second << "\n";
				}
			}
		}
	}
};

} // namespace

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cerr << "Usage: concept <file.vau> [--verbose] [--materialize-optionals]\n";
		return 1;
	}
	std::string path = argv[1];
	bool verbose = false;
	bool materializeOpt = false;
	for (int i = 2; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--verbose") verbose = true;
		else if (arg == "--materialize-optionals") materializeOpt = true;
		else {
			std::cerr << "Unknown flag: " << arg << "\n";
			return 1;
		}
	}

	try {
		std::vector<Line> lines;
		read_file(path, lines);
		Parser parser(lines);
		auto program = parser.parse();
		Interpreter interp(verbose, materializeOpt);
		interp.run(program);
	} catch (const std::exception &ex) {
		std::cerr << "Error: " << ex.what() << "\n";
		return 1;
	}

	return 0;
}
