//
//  main.cpp
//  interpreter
//
//  Created by Daniel Rehman on 1902096.
//  Copyright © 2019 Daniel Rehman. All rights reserved.
//


/**
 
 Manifest: ----------------------------
 
 - i want to speed of C/C++,
 - the safety of Rust,
 - the style of Swift,
 - the types of Haskell,
 - the flexibility of lisp,
 - and the readability of Python.

 --------------------------------------
 
 
 
 
 
  ------------ TODOS: for this interpreter: ---------------------------------------
 
 
    - make it so that the lien doesnt go over when we type too many characters on one line.
    - - make it so it simply creates a newline, and wraps, it, and makes it pretty.
    - - or simply allow our draw function to now draw the rest of the line.
    - - - and then we will simply scroll over the left when we have a column that is out of place.
    - - - - this might involve adding a off_set to all lines when we print them, so we only start printing from a particular offset.
    - - - that might work.
 
 

 
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <thread>
#include <codecvt>
#include <tuple>
#include <iomanip>

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "color.h"
#include "terminal_manip.hpp"

// Parameters:

#define language_name   "nostril"

const std::string welcome_message = BRIGHT_GREEN "A " language_name " Interpreter." RESET GRAY "\n\t[Created by Daniel Rehman.]\n(type \":help\" for more info.)" RESET;

const size_t number_of_spaces_for_tab = 4;

std::string code_prompt = BRIGHT_GREEN " ║  " RESET;
std::string command_prompt = CYAN " ╚╡ " RESET;
std::string output_prompt = GRAY "       :   " RESET;




std::vector<std::pair<std::string, std::vector<std::string>>> input = {{"", {}}};

size_t column = 0;
size_t line = 1;

bool quit = false;
bool paused = false;


void process_arrow_key() {
    char direction = getch();
    
    if (direction == 65) { // up
        if (line > 1) {
            line--;
            if (column >= input[line - 1].first.size()) column = input[line - 1].first.size();
        }
        
    } else if (direction == 66) { // down
        if (line < input.size()) {
            line++;
            if (column >= input[line - 1].first.size()) column = input[line - 1].first.size();
        }
        
    } else if (direction == 67) { // right
        if (column < input[line - 1].first.size()) column++;
        else if (column == input[line - 1].first.size() && line < input.size()) {column = 0; line++;}
        
    } else if (direction == 68) { // left
        if (column > 0) column--;
        else if (column == 0 && line > 1) column = input[--line - 1].first.size();
    }
}


void print_tab() {
    for (size_t i = number_of_spaces_for_tab; i--;)
        input[line - 1].first.insert(column++, 1, ' ');
}

void print_character(char c) {
    input[line - 1].first.insert(column++, 1, c);
    
    if (c == 'q') paused = true;
}

void delete_character() {
    if (column > 0) {
        input[line - 1].first.erase(input[line - 1].first.begin() + --column);
    } else if (column == 0 && line > 1) {
        const std::string current_line = input[line - 1].first;
        input.erase(input.begin() + --line);
        column = input[line - 1].first.size();
        input[line - 1].first.insert(input[line - 1].first.size(), current_line);
    }
}

void print_newline() {
    if (column == input[line - 1].first.size() && line == input.size()) { // if we are at the last column, and the last line in the file,
        input.push_back({"", {}});
        line++; column = 0;
    } else if (column < input[line - 1].first.size()) {
        const std::string new_line = input[line - 1].first.substr(column);
        input[line - 1].first.erase(column);
        input.insert(input.begin() + line, {new_line, {}});
        line++; column = 0;
    }
}


/**
 
 |msg
 |asdf
 
 
 */

void get_input() {
    
    while (!quit) {
        if (paused) {sleep(1); continue;}
        char c = getch();
        if (c == '\n') {
            print_newline();
        } else if (c == 9) { // tab
            print_tab();
        } else if (c == 127) { // backspace
            delete_character();
        } else if (c >= ' ' && c < 127) { // normal printable ascii char
            print_character(c);
        } else if (c == 27) { // escape
            char d = getch();
            if (d == 91) process_arrow_key();
            else if (d == 27) quit = true;
        }
        usleep(1000);
    }
}

void draw() {
    while (!quit) {
        if (paused) {sleep(1); continue;}
        clear_screen();
        int i = 0;
        for (auto s : input) {
            std::cout << s.first;
            if (++i < input.size()) std::cout << "\n";
        }
        
        const size_t length_of_current_one = input[line - 1].first.size();
        const size_t length_of_last_one = input[input.size() - 1].first.size();
        if (length_of_last_one - length_of_current_one > 0) {
            move_cursor_backwards((int)length_of_last_one - (int)length_of_current_one);
        }
        move_cursor_backwards((int) input[line - 1].first.size() - (int) column);
        move_cursor_upwards((int) input.size() - (int) line);
        fflush(stdout);
        usleep(25000);
    }
}

void interpret() { // this function does syntax highlighting as well.
    while (!quit) {
        for (auto& s : input) {
            if (s.first == "PASTA") s.second = {"linguini."};
            
            // note, the print statements always have a
            // newline at the end, forcibly.
            
        }
        if (paused) {
            clear_screen();
            std::cout << "lines: {" << std::endl;
            for (auto s : input) {
                std::cout << "\"" << s.first << "\"," << std::endl;
            }
            std::cout << "}" << std::endl;
            std::cout << "line = " << line << ", col = " << column << std::endl;
            getch();
            clear_screen();
            paused = false;
        }
        
        sleep(1);
    }
}

int main() {
    std::cout << welcome_message << std::endl;
    std::thread draw_thread = std::thread(draw);
    std::thread interpret_thread = std::thread(interpret);
    get_input();
    interpret_thread.detach();
    draw_thread.join();
    return 0;
}
