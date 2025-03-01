/**
 * @file touch.cpp
 * @brief Implements the touch command for Windows 11.
 *
 * This file implements a Windows version of the Unix touch command, which creates
 * a new file and populates it with header information and additional code snippets
 * based on a configuration file.
 *
 * @author 
 *   Gustav Pettersson Björklund
 * @date 2025-03-01
 * @details Released as part of the Windows 11 development package.
 */

#include <stdio.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>  // For strcmp
#include <ctime>    // For time_t
#include <windows.h>// For GetModuleFileName

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define VERSION "(Windows 11) 1.0.0"
#define CONFIG_PATH "./touch.conf"

#undef DEBUG

// Macros for printing:
//
// ERROR_PRINT: for error messages (always printed)
// INFO_PRINT: for important informational messages (always printed)
// DEBUG_PRINT: for noncritical debugging messages (only printed when DEBUG is defined)
#define ERROR_PRINT(...) fprintf(stderr, __VA_ARGS__)
#define INFO_PRINT(...) printf(__VA_ARGS__)
#ifdef DEBUG
  #define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
  #define DEBUG_PRINT(...) ((void)0)
#endif

/**
 * @brief Global map storing variables set via the configuration file.
 */
std::unordered_map<std::string, std::string> variable_map;

/**
 * @brief Structure representing an option.
 */
struct option {
  std::string identifier; /**< The identifier of the option. */
  bool is_prepend;        /**< Flag indicating whether the option should be prepended. */
};

/**
 * @brief Map of loaded options, grouped by type.
 *
 * Each key is a type (for example, a file extension or ".all" for defaults),
 * and the value is a vector of options for that type.
 */
std::unordered_map<std::string, std::vector<option>> type_options_map;

/**
 * @brief Vector to store raw code lines from the configuration file.
 */
std::vector<std::string> raw_code;

/**
 * @brief Map of reserved option names.
 *
 * Currently not used in the implementation, but may be used for future validation.
 */
const std::unordered_map<std::string, bool> reserved_names = {
  {"<date>", true},
  {"<file>", true},
};

/**
 * @brief Map of file extensions and their expected comment strings.
 */
const std::unordered_map<std::string, std::string> comment_str_map = {
    {".c",    "// "},
    {".cpp",  "// "},
    {".h",    "// "},
    {".hpp",  "// "},
    {".py",   "# "},
    {".java", "// "},
    {".js",   "// "},
    {".ts",   "// "},
    {".rb",   "# "},
    {".go",   "// "},
    {".rs",   "// "},
    {".cs",   "// "},
    {".php",  "// "},
    {".swift","// "},
    {".kt",   "// "},
    {".scala","// "},
    {".sh",   "# "},
    {".pl",   "# "},
    {".r",    "# "},
    {".lua",  "-- "},
    {".sql",  "-- "},
    {".asm",  "; "},
    {".s",    "; "},
    {".vb",   "' "},
    {".vba",  "' "},
    {".m",    "// "},  // Objective-C (or ambiguous with MATLAB)
    {".mm",   "// "},  // Objective-C++
    {".erl",  "% "},
    {".ex",   "# "},
    {".exs",  "# "},
    {".hs",   "-- "},
    {".lisp", ";; "},
    {".clj",  ";; "},
    {".scm",  ";; "},
    {".f90",  "!"},
    {".f95",  "!"},
    {".f03",  "!"},
    {".ada",  "-- "},
    {".pas",  "// "},
    {".dart", "// "},
    {".coffee","# "},
    {".groovy","// "},
    {".nim",  "# "},
    {".rkt",  "; "},
    {".vhd",  "-- "},
    {".vhdl", "-- "},
    {".pro",  "% "},
    {".sml",  "(* "},  // Standard ML uses (* ... *) for block comments.
    {".ml",   "(* "},  // OCaml uses the same syntax.
    {".bat",  "REM "},
    {".ps1",  "# "}
};

/**
 * @brief Retrieves the directory path of the current executable.
 *
 * This function calls the Win32 API function GetModuleFileName with a NULL module handle
 * to obtain the full path of the executable, then extracts the directory portion.
 *
 * @return A string containing the directory path of the executable.
 */
std::string get_exe_path() {
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    std::string::size_type pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(0, pos) : "";
}

/**
 * @brief Constructs the full configuration file path.
 *
 * This function concatenates the executable's directory path with the defined CONFIG_PATH.
 *
 * @return A string containing the full path to the configuration file.
 */
std::string get_config_path() {
    return get_exe_path() + "\\" + CONFIG_PATH;
}

/**
 * @brief Retrieves the current date in YYYY-MM-DD format.
 *
 * This function uses the C time API to obtain the current local date and then formats it.
 *
 * @return A string representing the current date.
 */
std::string get_current_date() {
  time_t now = time(0);
  struct tm timeinfo;
  char buffer[80];
  localtime_s(&timeinfo, &now);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return std::string(buffer);
}

/**
 * @brief Converts an option identifier to its final output form.
 *
 * Special option identifiers (such as "<date>" and "<file>") are replaced with their
 * corresponding values. If an option exists in the variable map, that value is returned.
 * Otherwise, the original option is returned.
 *
 * @param option The option identifier to convert.
 * @param filename The filename used for substituting the "<file>" option.
 * @return A string containing the converted option.
 */
std::string convert_option(const std::string &option, const std::string &filename) {
  if (option == "<date>") {
    return "DATE: " + get_current_date();
  }
  else if (option == "<file>") {
    return "FILE: " + filename;
  }
  else {
    if (variable_map.find(option) != variable_map.end()) {
      return variable_map[option];
    }
    else {
      return option;
    }
  }
}

/**
 * @brief Parses the configuration file.
 *
 * This function reads the configuration file line by line and processes various commands:
 * - "SET" commands to define variables.
 * - "<type ...>" commands to declare option types.
 * - "<prepend>", "<append>", and "<raw>" markers to set context for options.
 *
 * Options are stored in the global maps for later processing.
 *
 * @param filename The path to the configuration file.
 * @param file_extension The file extension of the target file, used to determine which options are relevant.
 */
void parse_config_file(const char *filename, const std::string &file_extension) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    ERROR_PRINT("Error: Could not open configuration file %s\n", filename);
    return;
  }

  std::string line;
  std::string current_type = "";
  bool is_prepend = false;
  bool is_raw = false;
  while (std::getline(file, line)) {
    // Trim leading and trailing whitespace.
    line.erase(0, line.find_first_not_of(" \t"));
    line.erase(line.find_last_not_of(" \t") + 1);

    // Skip empty lines.
    if (line.empty()) continue;

    if (line.find("SET ") == 0) {
      // Process SET commands.
      size_t pos = 4;
      size_t equal_pos = line.find('=', pos);
      if (equal_pos == std::string::npos) {
        ERROR_PRINT("Error: Invalid SET command syntax: %s\n", line.c_str());
        continue;
      }
      std::string var_name = line.substr(pos, equal_pos - pos);
      var_name.erase(0, var_name.find_first_not_of(" \t"));
      var_name.erase(var_name.find_last_not_of(" \t") + 1);
      
      std::string var_value = line.substr(equal_pos + 1);
      var_value.erase(0, var_value.find_first_not_of(" \t"));
      var_value.erase(var_value.find_last_not_of(" \t") + 1);

      // Remove surrounding quotes if present.
      if (!var_value.empty() && var_value.front() == '"' && var_value.back() == '"') {
        var_value = var_value.substr(1, var_value.size() - 2);
      }

      // Store variables with angle brackets for easy substitution.
      variable_map["<" + var_name + ">"] = var_value;
      continue;
    }
    else if (line.find("<type ") == 0) {
      // Extract type name.
      current_type = line.substr(6, line.size() - 7);
      is_prepend = false;
      is_raw = false;
      if (type_options_map.find(current_type) == type_options_map.end()) {
        type_options_map[current_type] = std::vector<option>();
      }
      DEBUG_PRINT("Found type: %s\n", current_type.c_str());
      continue;
    }
    else if (line.find("<prepend>") == 0) {
      is_prepend = true;
      DEBUG_PRINT("Found prepend\n");
      continue;
    }
    else if (line.find("<append>") == 0) {
      is_prepend = false;
      DEBUG_PRINT("Found append\n");
      continue;
    }
    else if (line.find("<raw>") == 0) {
      is_raw = true;
      DEBUG_PRINT("Found raw\n");
      continue;
    }
    else {
      // Treat any other line as an option if inside a type block.
      if (current_type.empty()) {
        ERROR_PRINT("Error: Option %s is not inside a type block\n", line.c_str());
      } 
      else if (is_raw && current_type == file_extension) {
        DEBUG_PRINT("Found raw option: %s\n", line.c_str());
        raw_code.push_back(line);
      }
      else {
        DEBUG_PRINT("Found option: %s\n", line.c_str());
        type_options_map[current_type].push_back({line, is_prepend});
      }
    }
  }
}

/**
 * @brief Prints help information to the console.
 *
 * This function outputs usage instructions, available command-line options,
 * and repository information for the touch command.
 */
void print_help() {
  INFO_PRINT("Usage: touch FILE...\n");
  INFO_PRINT("Customize the touch command using the configuration file %s\n", get_config_path().c_str());
  INFO_PRINT("\nOptions:\n");
  INFO_PRINT("  --version  Display version information\n");
  INFO_PRINT("  --help     Display this help message\n\n");
  INFO_PRINT("touch.exe is a private non-commercial project bundled with win_dev_tools by Gustav Pettersson Björklund.\n");
  INFO_PRINT("This program comes with NO WARRANTY. If you are missing some functionality feel free to contribute :D \n");
  INFO_PRINT("For feature requests or issues, please create an issue on the GitHub repository:\n");
  INFO_PRINT("https://github.com/GustavPetterssonBjorklund/win_dev_tools\n");
}

/**
 * @brief Prompts the user for confirmation before performing an action.
 *
 * This function displays a prompt asking whether the user wants to perform
 * a specific action. The user must enter a confirmation phrase to proceed.
 *
 * @param action A description of the action that requires confirmation.
 * @param confirmation_type_phrase The phrase the user must type to confirm.
 * @return True if the user confirms the action, false otherwise.
 */
bool confirm_action(const char* action, const char* confirmation_type_phrase) {
  INFO_PRINT("Do you want to %s? [y/N] ", action);
  char response;
  scanf_s(" %c", &response, 1);
  if (response == 'y' || response == 'Y') {
    INFO_PRINT("Please type \"%s\" to confirm that you want to %s: ", confirmation_type_phrase, action);
    char confirmation[100];
    scanf_s("%s", confirmation, (unsigned)sizeof(confirmation));
    return (strcmp(confirmation, confirmation_type_phrase) == 0);
  }
  return false;
}

/**
 * @brief The main entry point for the touch command.
 *
 * This function processes command-line arguments, checks for file existence,
 * parses the configuration file, gathers options and raw code, builds the final
 * file message, and writes the message to the target file.
 *
 * @param argc The number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return EXIT_SUCCESS if the program completes successfully, otherwise EXIT_FAILURE.
 */
int main(int argc, char *argv[]) {
  std::string file_extension;
  std::string filename;

  if (argc > 1) {
    if (strcmp(argv[1], "--version") == 0) {
      INFO_PRINT("touch %s\n", VERSION);
      return EXIT_SUCCESS;
    }
    else if (strcmp(argv[1], "--help") == 0) {
      print_help();
      return EXIT_SUCCESS;
    }
    else {
      FILE *file = nullptr;
      filename = argv[1];
      // Check if file exists.
      if (fopen_s(&file, argv[1], "r") == 0 && file != nullptr) {
        ERROR_PRINT("Error: File %s already exists\n", argv[1]);
        fclose(file);
        if (!confirm_action("overwrite the file", "overwrite")) {
          INFO_PRINT("Aborting file creation...\n");
          return EXIT_FAILURE;
        }
      }
      DEBUG_PRINT("Creating file: %s\n", argv[1]);
      if (fopen_s(&file, argv[1], "w") != 0 || file == nullptr) {
        ERROR_PRINT("Error: Could not create file %s\n", argv[1]);
        return EXIT_FAILURE;
      }
      else {
        size_t dot_pos = filename.find_last_of(".");
        file_extension = (dot_pos != std::string::npos) ? filename.substr(dot_pos) : "";
        DEBUG_PRINT("Created file of type %s: %s\n", file_extension.c_str(), argv[1]);
      }
      fclose(file);
    }
  }
  else {
    ERROR_PRINT("Error: No file name provided\n");
    print_help();
    return EXIT_FAILURE;
  }

  // Parse configuration file from the executable's directory.
  std::string config_path = get_config_path();
  parse_config_file(config_path.c_str(), file_extension);

  // Gather options based on file extension.
  std::vector<std::string> prepend_options;
  std::vector<std::string> append_options;
  std::vector<std::string> default_options;
  if (type_options_map.find(file_extension) != type_options_map.end()) {
    for (auto const &opt : type_options_map[file_extension]) {
      if (opt.is_prepend) {
        DEBUG_PRINT("Prepending option: %s\n", opt.identifier.c_str());
        prepend_options.push_back(opt.identifier);
      }
      else {
        DEBUG_PRINT("Appending option: %s\n", opt.identifier.c_str());
        append_options.push_back(opt.identifier);
      }
    }
  }
  else {
    DEBUG_PRINT("Error: No configuration found for file type %s\n", file_extension.c_str());
  }

  if (type_options_map.find(".all") != type_options_map.end()) {
    for (auto const &opt : type_options_map[".all"]) {
      DEBUG_PRINT("Adding default option: %s\n", opt.identifier.c_str());
      default_options.push_back(opt.identifier);
    }
  }

  // Merge options in the correct order.
  std::vector<std::string> all_options;
  all_options.insert(all_options.end(), prepend_options.begin(), prepend_options.end());
  all_options.insert(all_options.end(), default_options.begin(), default_options.end());
  all_options.insert(all_options.end(), append_options.begin(), append_options.end());

  // Get comment string for the file extension.
  std::string comment_str = (comment_str_map.find(file_extension) != comment_str_map.end()) ?
                            comment_str_map.at(file_extension) : "// ";

  // Build final file message.
  std::string file_message;
  for (const auto &opt : all_options) {
    std::string converted_option = convert_option(opt, filename);
    DEBUG_PRINT("%s%s\n", comment_str.c_str(), converted_option.c_str());
    file_message += comment_str + converted_option + "\n";
  }

  // Add raw code.
  if (!raw_code.empty()) {
    // Add a newline before the raw code.
    file_message += "\n";
    for (const auto &line : raw_code) {
      DEBUG_PRINT("%s\n", line.c_str());
      // Remove any surrounding single or double quotes.
      std::string trimmed = line;
      if (!trimmed.empty() && (trimmed.front() == '\"' || trimmed.front() == '\'')) {
        trimmed = trimmed.substr(1, trimmed.size() - 2);
      }
      if (trimmed == "\\n") {
        file_message += "\n";
      }
      else {
        file_message += line + "\n";
      }
    }
  }

  // Write the message to the file.
  FILE *file_out = nullptr;
  if (fopen_s(&file_out, argv[1], "w") != 0 || file_out == nullptr) {
    ERROR_PRINT("Error: Could not open file %s for writing\n", argv[1]);
    return EXIT_FAILURE;
  }
  fprintf(file_out, "%s", file_message.c_str());
  fclose(file_out);

  return EXIT_SUCCESS;
}

