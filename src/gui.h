#ifndef GUI_H
#define GUI_H

#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <functional>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Box.H>

class GUI {

public:
    GUI();
    ~GUI();

    void init(std::function<void()> callback=nullptr);
    int run();
    void output(const char* s, size_t n);
    void deactivate();
    void activate();
    void setInfo(const std::string& label);
    bool updateArgs();

private:
    static void onTimeout(void* data);

private:
    std::function<void()> callback;
    std::unique_ptr<Fl_Window> window;
    std::unique_ptr<Fl_Input> input_port;
    std::unique_ptr<Fl_Input> input_fps;
    std::unique_ptr<Fl_Input> input_timeout;
    std::unique_ptr<Fl_Choice> choice_quality;
    std::unique_ptr<Fl_Button> button_start;
    std::unique_ptr<Fl_Output> output_status;
    std::unique_ptr<Fl_Output> info;
    std::unique_ptr<Fl_Text_Buffer> text_buf;
    std::unique_ptr<Fl_Text_Display> display;
};


class GuiBuf : public std::streambuf {

public:
    GuiBuf(GUI* gui): gui(gui) {}

protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::cerr << s;
        gui->output(s, n);
        return n;
    }

private:
    GUI *gui;
};


#endif // GUI_H