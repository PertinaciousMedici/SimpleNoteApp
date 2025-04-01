#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <limits>
#include <istream>
#include <thread>
#include <chrono>
#include <iosfwd>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <fstream>

using std::string;
using std::vector;

namespace global_vars
{
	const string CSV_FILE_NAME{ "note_store" };
	const string INSTRUCTION_PROMPT{ "\x1b[1;33m[Choice]:\x1b[0m What would you like to do? " };
	const vector<string> OPTIONS{"c", "w", "r", "e"};
}

namespace options
{
	
	enum actions
	{
		check = 'c',
		write = 'w',
		remove = 'r',
		exit = 'e',
	};

	static void handleAction(int signal)
	{
		std::cout << "\n\x1b[1;32m[EXIT]:\x1b[0m Cleaning up... ";
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::exit(signal);
	}

	static void printInstructions()
	{
		std::cout << "-------------- \x1b[1;34m[Options]\x1b[0m --------------\n";
		std::cout << "\x1b[1;33m[Display]:\x1b[0m Display all saved notes. Press c.\n";
		std::cout << "\x1b[1;33m[Write]:\x1b[0m Write and save a new note. Press w.\n";
		std::cout << "\x1b[1;33m[Delete]:\x1b[0m Delete an existing note. Press r.\n";
		std::cout << "\x1b[1;33m[Exit]:\x1b[0m Exit the program. Press e.\n";
		std::cout << "---------------------------------------\n";
		std::cout << std::flush;
	}
}

namespace utilities
{

	const int _MAX_INTEGER = std::numeric_limits<int>::max();

	static int closest(int target, int min, int max)
	{
		return (std::abs(target - min) <= std::abs(target - max)) ? min : max;
	}

	static string lower(const string& original)
	{
		string result = original;
		std::transform(result.begin(), result.end(), result.begin(), ::tolower);
		return result;
	}

	static string format(int integer, int padding)
	{
		string as_string = std::to_string(integer);

		while (as_string.length() < padding)
		{
			as_string = '0' + as_string;
		}

		return as_string;
	}

	static int safe_stoi(const string& str)
	{
		try
		{
			return std::stoi(str);
		}
		catch (const std::invalid_argument&)
		{
			std::cout << "\x1b[1;31m[ERROR]:\x1b[31m Invalid input! Not a valid number.\x1b[0m\n";
		}
		catch (const std::out_of_range&)
		{
			std::cout << "\x1b[1;31m[ERROR]:\x1b[31m Invalid input! Number out of range.\x1b[0m\n";
		}
		
		return _MAX_INTEGER;
	}
};

struct Note
{
	int code;
	string title;
	string description;
};

using NotePtr = std::shared_ptr<Note>;

struct NoteNode
{
	NotePtr this_note;
	NoteNode* prior_ref;
	NoteNode* next_ref;

	void explode()
	{
		this_note.reset();
	}
};

struct LinkedList
{
	NoteNode* head = nullptr;
	NoteNode* last = nullptr;
	size_t length = 0;

	NotePtr search(int code) const
	{
		NoteNode* current = head;

		while (current && current->this_note->code != code)
		{
			current = current->next_ref;
		}

		return current ? current->this_note : nullptr;
	}

	void push_node(NotePtr new_note)
	{
		NoteNode* new_node = new NoteNode{};
		new_node->this_note = new_note;
		new_node->next_ref = nullptr;

		if (head && last)
		{
			new_node->prior_ref = last;
			last->next_ref = new_node;
			last = new_node;
		}
		else
		{
			head = new_node;
			last = new_node;
		}

		length++;
		sanitize_codes();
	}

	void delete_note(int code)
	{
		if (!head || !last) return;

		int min = head->this_note->code;
		int max = last->this_note->code;

		int closest = utilities::closest(code, min, max);

		NoteNode* current = (closest == min) ? head : last;

		while (current && current->this_note->code != code)
		{
			current = (closest == min) ? current->next_ref : current->prior_ref;
		}

		if (current)
		{
			if (current->prior_ref)
			{
				current->prior_ref->next_ref = current->next_ref;
			}

			if (current->next_ref)
			{
				current->next_ref->prior_ref = current->prior_ref;
			}

			if (current == head) head = current->next_ref;
			if (current == last) last = current->prior_ref;

			current->explode();
			delete current;
			current = nullptr;
			length--;
		}
		
		sanitize_codes();
	}

	void sanitize_codes(void) const
	{
		NoteNode* current = head;
		int value = 1;

		while (current)
		{
			current->this_note->code = value;
			current = current->next_ref;
			value = value + 1;
		}
	}

	vector<string> fetchCodes(void) const
	{
		vector<string> return_vector{};
		NoteNode* current = head;

		while (current)
		{
			string push_str = std::to_string(current->this_note->code);
			return_vector.push_back(push_str);
			current = current->next_ref;
		}

		return return_vector;
	}

	int yieldCode() const
	{
		return length + 1;
	}
};

using ListPtr = std::shared_ptr<LinkedList>;

ListPtr create_list(void);
NotePtr create_note(int code, const string& name, const string& description);
static string fetch_input(const string& prompt, const vector<string>& expected);
static void display_all(const ListPtr& list);
static void prompt_create_note(ListPtr& list);
static void prompt_delete_note(ListPtr& list);

namespace store_manipulation
{
	static void writeCSV(const ListPtr& list, const string& filename)
	{
		std::ofstream file(filename);

		if (!file.is_open())
		{
			std::cout << "\x1b[1;31m[ERROR]:\x1b[31m Failed to open the file to write.\x1b[0m\n";
			return;
		}

		NoteNode* current = list->head;

		while (current)
		{
			file << current->this_note->code << ","
				<< current->this_note->title << ","
				<< current->this_note->description << "\n";
			current = current->next_ref;
		}

		file.close();
	}

	static void readCSV(ListPtr& list, const string& filename)
	{
		std::ifstream file(filename);

		if (!file.is_open())
		{
			std::cout << "\x1b[1;31m[ERROR]:\x1b[31m Failed to open the file to read.\x1b[0m\n";
			std::ofstream newFile(filename);
			newFile.close();
			return;
		}

		string line;
		while (std::getline(file, line))
		{
			std::stringstream ss(line);
			string codeStr, title, description;

			if (!std::getline(ss, codeStr, ',') || !std::getline(ss, title, ',') || !std::getline(ss, description))
				continue;

			int code = utilities::safe_stoi(codeStr);
			if (code == utilities::_MAX_INTEGER) continue;

			NotePtr new_note = create_note(code, title, description);
			list->push_node(new_note);
		}

		file.close();
	}
};

int main(void)
{
	ListPtr application_list = create_list();
	store_manipulation::readCSV(application_list, global_vars::CSV_FILE_NAME);
	
prompt:
	options::printInstructions();
	string choice = fetch_input(global_vars::INSTRUCTION_PROMPT, global_vars::OPTIONS);
	int choosen_option = (int)choice[0];

	if (choosen_option == options::actions::check)
	{
		display_all(application_list);
	}

	if (choosen_option == options::actions::write)
	{
		prompt_create_note(application_list);
	}
	
	if (choosen_option == options::actions::remove)
	{
		prompt_delete_note(application_list);
	}

	if (choosen_option == options::actions::exit)
	{
		store_manipulation::writeCSV(application_list, global_vars::CSV_FILE_NAME);
		options::handleAction(15);
	}

	std::cout << "\x1b[1;33mPress c to continue.\x1b[0m\n";
	string input = fetch_input("... ", { "c" });
	system("cls");
	goto prompt;
}

ListPtr create_list(void)
{
	return std::make_shared<LinkedList>(LinkedList{});
}

NotePtr create_note(int code, const string& name, const string& description)
{
	return std::make_shared<Note>(Note{ code, name, description });
}

static string fetch_input(const string& prompt, const vector<string>& expected)
{
	string return_value;

validate:
	do
	{
		std::cout << prompt;
		std::getline(std::cin >> std::ws, return_value);

		if (std::cin.fail())
		{
			std::cout << "\x1b[1;31m[ERROR]\x1b[0m \x1b[31mInvalid input! Please insert a valid option.\x1b[0m";
			std::cin.clear();
			std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			goto validate;
		}

		return_value = utilities::lower(return_value);
	} while (return_value.empty());

	if (!expected.empty())
	{
		bool valid_input{ false };

		for (const auto& option : expected)
		{
			if (return_value == option)
			{
				valid_input = true;
				break;
			}
		}

		if (!valid_input)
		{
			std::cout << "\x1b[1;31m[ERROR]\x1b[0m \x1b[31mInvalid input! Please insert a valid option.\n\x1b[0m";
			goto validate;
		}
	}

	return return_value;
}

static void display_all(const ListPtr& list)
{
	int counter{ 1 };

	if (list->length == 0 || !list->head) return;
	std::cout << '\n' << "\x1b[1;34m[Display]:\x1b[0m" << "\n\n";

	NoteNode* CurrentNode = list->head;

display:
	std::cout << "-------------- " << "\x1b[1;34m[Note  " << counter << "]\x1b[0m --------------" << '\n';
	std::cout << "\x1b[1;33m[Code]:\x1b[0m " << utilities::format(CurrentNode->this_note->code, 5) << ".\n";
	std::cout << "\x1b[1;33m[Name]:\x1b[0m " << CurrentNode->this_note->title << '\n';
	std::cout << "\x1b[1;33m[Description]:\x1b[0m " << CurrentNode->this_note->description << '\n';
	counter++;

	if (CurrentNode->next_ref)
	{
		CurrentNode = CurrentNode->next_ref;
		goto display;
	}

	std::cout << "---------------------------------------" << '\n' << std::flush;
}

static void prompt_create_note(ListPtr& list)
{
	std::cout << '\n';
	std::cout << "---------- \x1b[1;34m[Note Insertion]\x1b[0m -----------" << '\n';
	string name = fetch_input("\x1b[1;33m[Name]:\x1b[0m What is the name of the note? ", {});
	string description = fetch_input("\x1b[1;33m[Description]:\x1b[0m What is the description of the note? ", {});

	int current_code = list->yieldCode();
	NotePtr new_note = create_note(current_code, name, description);
	list->push_node(new_note);

	std::cout << "\x1b[1;33m[Inserted]:\x1b[0m \x1b[1;42m" << name << "\x1b[0m at position " << current_code << ".\n";
	std::cout << "---------------------------------------" << '\n' << std::flush;
}

static void prompt_delete_note(ListPtr& list)
{
	vector<string> list_options = list->fetchCodes();

	std::cout << '\n';
	std::cout << "----------- \x1b[1;34m[Note Deletion]\x1b[0m -----------" << '\n';
prompt:
	string code = fetch_input("\x1b[1;33m[Code]:\x1b[0m What is the code of the note you'd like to delete?\x1b[0m ", list_options);
	int as_int = utilities::safe_stoi(code);

	if (as_int == utilities::_MAX_INTEGER) goto prompt;

	if (list->search(as_int))
	{
		list->delete_note(as_int);
		std::cout << "\x1b[1;33m[Deletion]:\x1b[0m Successfully deleted the note.\n";
	}
	else
	{
		std::cout << "\x1b[1;33m[Deletion]:\x1b[0m Note not found.\n";
	}
	
	std::cout << "---------------------------------------" << '\n' << std::flush;
}

