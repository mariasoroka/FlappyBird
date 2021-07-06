#include "Engine.h"
#include <stdlib.h>
#include <vector>
#include <memory.h>
#include <list>
#include <random>
#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#define color 16777215 //white color
using namespace std;
//class for obstacles parameters
class obstacle {
public:
    obstacle(int obst_pos_, int obst_height_, int gate_height_) :obst_pos(obst_pos_), obst_lower_bound(obst_height_), obst_upper_bound(obst_height_ + gate_height_) {}
    bool check_collision(int h_pos, int v_pos) {
        if (h_pos > obst_pos && h_pos < obst_pos + obst_width &&
            (v_pos < obst_lower_bound || v_pos > obst_upper_bound)) {
            return true;
        }
        return false;
    }
    bool check_bird_collision(int bird_h_pos, int bird_height, int curr_pos, int bird_half_size) {
        if (check_collision(bird_h_pos + curr_pos + bird_half_size, bird_height + bird_half_size) ||
            check_collision(bird_h_pos + curr_pos - bird_half_size, bird_height + bird_half_size) ||
            check_collision(bird_h_pos + curr_pos - bird_half_size, bird_height - bird_half_size) ||
            check_collision(bird_h_pos + curr_pos + bird_half_size, bird_height - bird_half_size)) {
            return true;
        }
        return false;
    }
    int get_obst_pos() { return obst_pos; }
    int get_obst_lower_bound() { return obst_lower_bound; }
    int get_obst_upper_bound() { return obst_upper_bound; }
    int get_obst_end_pos() { return obst_pos + obst_width; }
private:
    int obst_pos;
    int obst_lower_bound;
    int obst_upper_bound;
    int obst_width = 100;
};

int bird_height, bird_h_pos; //bird heigth and horizontal position
int bird_half_size = 13; //bird size
float bird_up_speed, bird_up_acc; //bird speed and acceleration
int avg_gate_height = SCREEN_HEIGHT / 3; // average gate height
float range_gate_height = 0.2f; // gate height is in range [1 - range_gate_heigth, 1 + range_gate_heigth]
int speed = 500; // horizontal bird speed
int last_obst_pos = 0; // position of the last obstacle

int curr_pos, pos_change, score; //current game situation
bool in_game = false;
bool key_was_pressed = false;
bool first_game = true;

vector<vector<uint32_t>> digits; // images for digits
int score_number_width = 32; // width and height of digit picture
int score_number_height = 40;

uint32_t* buffer_to_fill;//buffer for drawing
uint32_t* start_window;//start screen buffer
list<obstacle> obstacles;//list of obstacles
auto active_obst_iterator = obstacles.begin(); 
auto last_obst_to_draw_iterator = obstacles.begin();

//this function reads start screen from file
void read_bmp_picture_from_file(string filename) {
    ifstream file_to_read;
    file_to_read.open(filename, ios::binary);
    file_to_read.seekg(54, ios::beg);
    int filesize = SCREEN_HEIGHT * SCREEN_WIDTH * 3;
    int number_of_pixels = SCREEN_HEIGHT * SCREEN_WIDTH;
    unsigned char* pixels = new unsigned char[filesize];
    file_to_read.read((char*)pixels, filesize);
    file_to_read.close();
    start_window = new uint32_t[number_of_pixels];
    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        for (int j = 0; j < SCREEN_WIDTH; j++) {
            int pixel_num_in_data = i * SCREEN_WIDTH + j;
            int pixel_num_for_picture = number_of_pixels - (i * SCREEN_WIDTH + (SCREEN_WIDTH - j - 1)) - 1;
            start_window[pixel_num_for_picture] = pixels[pixel_num_in_data * 3] * 65536 + pixels[pixel_num_in_data * 3 + 1] * 256 + pixels[pixel_num_in_data * 3 + 2];
        }
    }
    delete[] pixels;
}

//this function reads digits for score from files
void read_digits_from_file(string folder) {
    digits = vector<vector<uint32_t>>(10, vector<uint32_t>(score_number_height * score_number_width, color));
    unsigned char* value = new unsigned char[3 * score_number_height * score_number_width];
    for (int k = 0; k < 10; k++) {
        ifstream file_to_read;
        file_to_read.open(folder + to_string(k) + ".bmp", std::ios::binary);
        file_to_read.seekg(0, std::ios::end);
        int filesize = file_to_read.tellg();
        file_to_read.seekg(218, ios::beg);
        file_to_read.read((char*)value, 3 * score_number_height * score_number_width);
        for (int i = 0; i < score_number_height; i++) {
            for (int j = 0; j < score_number_width; j++) {
                int pixel_num_in_data = i * score_number_width + j;
                int pixel_num_for_picture = score_number_height * score_number_width - (i * score_number_width + (score_number_width - j - 1)) - 1;
                if (value[pixel_num_in_data * 3] < 30 || value[pixel_num_in_data * 3 + 1] < 30 || value[pixel_num_in_data * 3 + 2] < 30) {
                    digits[k][pixel_num_for_picture] = 0;
                }
            }
        }
        file_to_read.close();
    }
    delete[] value;
}

//this function prints score to buffer
void draw_score(int h_pos, int v_pos, int score, uint32_t(*buffer)[1024]) {
    int digit;
    int digit_number = 0;
    if (score == 0) {
        for (int i = 0; i < score_number_height; i++) {
            for (int j = 0; j < score_number_width; j++) {
                buffer[h_pos + i][ v_pos + j] = digits[0][i * score_number_width + j];
            }
        }
    }
    else {
        while (score != 0) {
            digit = score % 10;
            for (int i = 0; i < score_number_height; i++) {
                for (int j = 0; j < score_number_width; j++) {
                    buffer[h_pos + i][ v_pos + j - score_number_width * digit_number] = digits[digit][i * score_number_width + j];
                }
            }
            score /= 10;
            digit_number++;
        }
    }
}
//game initialization
void initialize() {
    bird_height = SCREEN_HEIGHT / 2;
    curr_pos = 0;
    bird_h_pos = SCREEN_WIDTH / 4;
    bird_up_speed = 0;
    bird_up_acc = 3000;
    key_was_pressed = false;
    in_game = false;
    if (first_game == true) {
        buffer_to_fill = new uint32_t[SCREEN_HEIGHT * SCREEN_WIDTH];
        read_bmp_picture_from_file("./ImageFolder/start_window.bmp");
        read_digits_from_file("./ImageFolder/Digits/");
        first_game = false;
    }
    //generate first 10 obstacles
    default_random_engine generator(static_cast<long unsigned int>(time(0)));
    uniform_int_distribution<int> distribution_for_pos(200, 500);
    uniform_int_distribution<int> distribution_for_height(70, 400);
    uniform_int_distribution<int> distribution_for_gate_height((int)(avg_gate_height * (1.0f - range_gate_height)), (int)(avg_gate_height * (1.0f + range_gate_height)));
    int curr_obst_pos = 1024;
    for (int number_of_obst_for_init = 0; number_of_obst_for_init < 10; number_of_obst_for_init++) {
        curr_obst_pos += distribution_for_pos(generator);
        last_obst_pos = curr_obst_pos;
        obstacles.push_back(obstacle(curr_obst_pos, distribution_for_height(generator), distribution_for_gate_height(generator)));
    }
    active_obst_iterator = obstacles.begin();
    last_obst_to_draw_iterator = obstacles.begin();
    memset(buffer_to_fill, color, SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(uint32_t));
}
//restarts game
void another_try() {
    in_game = false;
    obstacles.clear();
    initialize();
}
//updates game data
void act(float dt) {
    //quit if escape was pressed
    if (is_key_pressed(VK_ESCAPE)) {
        in_game = false;
        schedule_quit_game();
    }
    //start to play
    if (is_key_pressed('S') && in_game == false) {
        score = 0;
        in_game = true;
    }
    if (in_game == true) {
        bird_up_speed += dt * bird_up_acc; //gravity
        if (is_key_pressed(VK_SPACE)) { //changes bird speed if key was pressed once
            if (key_was_pressed == false) {
                bird_up_speed = -1000; 
                key_was_pressed = true;
            }
        }
        else {
            key_was_pressed = false;
        }
        bird_height += int(bird_up_speed * dt); //calculate bird vertical position
        pos_change = int(speed * dt);
        curr_pos += pos_change;
        if (active_obst_iterator->get_obst_end_pos() < curr_pos + bird_h_pos - bird_half_size) {//check if there is any new obstacle to draw
            active_obst_iterator++;
            score++;
        }
        //collision check
        if (bird_height - bird_half_size < 0 || bird_height + bird_half_size > SCREEN_HEIGHT || active_obst_iterator->check_bird_collision(bird_h_pos, bird_height, curr_pos, bird_half_size)) {
            another_try();
        }
    }
}

void draw() {   
    if (in_game == true) {
        if (!obstacles.empty() && obstacles.front().get_obst_end_pos() < curr_pos) {
            obstacles.pop_front(); //if obstacle moved out of the screen, remove it from list
        }
        if (obstacles.size() <= 5) { // if there are less than 6 obstacles in the list, add 5 new obstacles
            for (int i = 0; i < 5; i++) {
                default_random_engine generator(static_cast<long unsigned int>(time(0)));
                uniform_int_distribution<int> distribution_for_pos(200, 500);
                uniform_int_distribution<int> distribution_for_height(70, 400);
                uniform_int_distribution<int> distribution_for_gate_height((int)(avg_gate_height * (1.0f - range_gate_height)), (int)(avg_gate_height * (1.0f + range_gate_height)));
                last_obst_pos += distribution_for_pos(generator);
                obstacles.push_back(obstacle(last_obst_pos, distribution_for_height(generator), distribution_for_gate_height(generator)));
                if (last_obst_pos < SCREEN_WIDTH) {
                    last_obst_to_draw_iterator++;
                }
            }
        }
        auto next_obst_iterator = last_obst_to_draw_iterator;
        next_obst_iterator++;
        if (next_obst_iterator != obstacles.end() && next_obst_iterator->get_obst_pos() < curr_pos + SCREEN_WIDTH - 1) { //if new obstacle should appear on the screen
            last_obst_to_draw_iterator++;
        }

        for (int i = 0; i < SCREEN_WIDTH - pos_change; i++) { // move everything in the buffer_to_fill to the left
            for (int j = 0; j < SCREEN_HEIGHT; j++) {
                buffer_to_fill[j * SCREEN_WIDTH + i] = buffer_to_fill[j * SCREEN_WIDTH + i + pos_change];
            }
        }
        for (int i = SCREEN_WIDTH - pos_change; i < SCREEN_WIDTH; i++) {// fill new pixels
            if (i + curr_pos > last_obst_to_draw_iterator->get_obst_pos() && i + curr_pos < last_obst_to_draw_iterator->get_obst_end_pos()) {
                for (int j = 0; j < SCREEN_HEIGHT; j++) {
                    int lower_bound = last_obst_to_draw_iterator->get_obst_lower_bound();
                    int upper_bound = last_obst_to_draw_iterator->get_obst_upper_bound();
                    if (j < lower_bound || j >  upper_bound) {
                        buffer_to_fill[j * SCREEN_WIDTH + i] = 0;
                    }
                    else {
                        buffer_to_fill[j * SCREEN_WIDTH + i] = color;
                    }
                }
            }
            else {
                for (int j = 0; j < SCREEN_HEIGHT; j++) {
                    buffer_to_fill[j * SCREEN_WIDTH + i] = color;
                }
            }
        }
        
        memcpy(buffer, buffer_to_fill, SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(uint32_t));
        for (int i = -bird_half_size; i <= bird_half_size; i++) { //draw the bird
            for (int j = -bird_half_size; j <= bird_half_size; j++) {
                buffer[bird_height + i][bird_h_pos + j] = 200;
            }
        }
        int score_v_pos = 50;
        int score_h_pos = 200;
        draw_score(score_v_pos, score_h_pos, score, buffer); //draw score
    }
    else {//if in_game == false
        memcpy(buffer, start_window, SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(uint32_t)); //draw start screen
        int score_v_pos = 500;
        int score_h_pos = 490;
        draw_score(score_v_pos, score_h_pos, score, buffer); //draw score
    }
}

// free game data in this function
void finalize()
{
    delete[] start_window;
    delete[] buffer_to_fill;
}

