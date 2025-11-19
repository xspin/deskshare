
#include "logger.h"
#include "utils.h"
#include "gui.h"
#include "pages.h"

GUI::GUI() {
}

GUI::~GUI() {
}

static void digitCheck(Fl_Widget* w, void* data) {
    Fl_Input* input = (Fl_Input*)w;
    const char* text = input->value();

    bool valid = true;
    for (int i = 0; text[i] != '\0'; ++i) {
        if (!isdigit(text[i])) {
            valid = false;
            break;
        }
    }

    input->textcolor(valid ? FL_BLACK : FL_RED);
    input->redraw();

    if (!valid) return;

    if (data && std::string((const char*)data) == "FPS") {
        int t = std::stoi(text);
        if (0 < t && t <= 60) {
            g_config.fps = t;
            pages::setup();
            LOG_INFO_STREAM << "Update FPS: " << g_config.fps;
        } else {
            input->textcolor(FL_RED);
            input->redraw();
            LOG_WARNING_STREAM << "Invalid FPS (1~60): " << t;
        }
    }
}

#define TIMEOUT_INTERVAL 3.0

void GUI::onTimeout(void* data) {
    GUI* self = static_cast<GUI*>(data);
    std::stringstream ss;
    ss << g_config.clients << "☺  ";
    ss << std::fixed << std::setprecision(1) << (g_config.frames / TIMEOUT_INTERVAL) << " fps  ";
    ss << utils::speedString(g_config.bytes / TIMEOUT_INTERVAL);

    self->output_status->value(ss.str().c_str());
    g_config.frames = 0;
    g_config.bytes = 0;

    Fl::repeat_timeout(TIMEOUT_INTERVAL, onTimeout, data);
}

void GUI::init(std::function<void()> callback) {
    this->callback = callback;
    int width = 500;
    int height = 400;

    int w = 50;
    int h = 20;

    int x = 20;
    int y = 20;

    int dx = w + 10;
    int dy = h + 10;

    window = std::make_unique<Fl_Window>(width, height, "DeskShare " APP_VERSION);
    window->position(500, 300);

    input_port = std::make_unique<Fl_Input>(x, y, w, h, "Port");
    input_port->align(FL_ALIGN_TOP);
    input_port->value(g_config.port);
    input_port->callback(digitCheck);

    input_timeout = std::make_unique<Fl_Input>(x + dx, y, w, h, "Timeout");
    input_timeout->align(FL_ALIGN_TOP);
    input_timeout->value(g_config.timeout);
    input_timeout->callback(digitCheck);

    input_fps = std::make_unique<Fl_Input>(x + dx*2, y, w, h, "FPS");
    input_fps->align(FL_ALIGN_TOP);
    input_fps->value(g_config.fps);
    input_fps->callback(digitCheck, (void*)"FPS");

    choice_quality = std::make_unique<Fl_Choice>(x + dx*3, y, w+8, h, "Quality");
    choice_quality->align(FL_ALIGN_TOP);
    for (int i = 1; i <= 10; ++i) {
        std::string label = std::to_string(i * 10) + "%";
        choice_quality->add(label.c_str());
    }
    choice_quality->value(g_config.quality * 10 - 1);
    choice_quality->callback([](Fl_Widget* w, void* data){
        Fl_Choice* choice = (Fl_Choice*)w;
        g_config.quality = (choice->value() + 1) / 10.0f;
        pages::setup();
        LOG_INFO_STREAM << "Update quality: " << g_config.quality;
    });

    output_status = std::make_unique<Fl_Output>(x + dx*4 + 10, y, w+w+w, h);
    output_status->align(FL_ALIGN_CENTER);
    output_status->value("");
    output_status->textsize(14);
    output_status->readonly(1);
    output_status->box(FL_FLAT_BOX);
    output_status->box(FL_FRAME_BOX);
    output_status->color(FL_BACKGROUND_COLOR);

    Fl::add_timeout(TIMEOUT_INTERVAL, onTimeout, this);

    button_start = std::make_unique<Fl_Button>(x + dx*7-10, y-5, w, h+10, "Start");
    button_start->callback([](Fl_Widget* w, void* data) {
        GUI* self = static_cast<GUI*>(data);
        Fl_Button* btn = static_cast<Fl_Button*>(w);
        btn->deactivate();
        self->callback();
        self->display->take_focus();
    }, this);

    y += dy;
    info = std::make_unique<Fl_Output>(5, y, width - 10, h);
    info->readonly(1);
    // info->box(FL_FLAT_BOX);
    info->box(FL_FRAME_BOX);
    info->color(FL_GRAY);
    info->value("DeskShare");
    info->align(FL_ALIGN_CENTER);
    info->textsize(14);
    
    y += dy;
    text_buf = std::make_unique<Fl_Text_Buffer>();
    display = std::make_unique<Fl_Text_Display>(5, y, width - 10, height - y - 5);
    display->buffer(text_buf.get());  
    display->color(FL_BLACK);       // 背景色设为黑色
    display->textcolor(FL_WHITE);   // 文字颜色设为白色
    display->textsize(14);          // 可选：设置字体大小
    display->textfont(FL_COURIER);  // 等宽字体（适合日志）
    display->take_focus();
}

void GUI::activate() {
    input_port->activate();
    input_timeout->activate();
    // input_port->textcolor(FL_BLACK);
    // input_timeout->textcolor(FL_BLACK);
    button_start->label("Start");
    button_start->activate();
    button_start->labelcolor(FL_DARK_GREEN);
    info->value("");
}

void GUI::deactivate() {
    // input_timeout->color(FL_GRAY0);
    input_port->deactivate();
    input_timeout->deactivate();
    // input_port->textcolor(FL_RED);
    button_start->label("Stop");
    button_start->activate();
    button_start->labelcolor(FL_RED);
}

void GUI::setInfo(const std::string& label) {
    info->value(label.c_str());
    info->textcolor(FL_BLUE);
}

void GUI::output(const char* s, size_t n) {
    text_buf->append(s, n);
    display->scroll(INT_MAX, 0);
}

bool GUI::updateArgs() {
    Fl_Color err_color = FL_DARK_RED;
    try {
        int t;
        t = std::stoi(input_port->value());
        if (t <= 0 || t > 65535) {
            info->value("Invalid Port! (1~65535)");
            goto FAILED;
        }
        g_config.port = t;

        t = std::stoi(input_fps->value());
        if (t <= 0 || t > 60) {
            info->value("Invalid FPS! (1~60)");
            goto FAILED;
        }
        g_config.fps = t;

        t = std::stoi(input_timeout->value());
        if (t < 10 || t > 86400) {
            info->value("Invalid Timeout! (10~86400)");
            goto FAILED;
        }
        g_config.timeout = t;

        g_config.quality = (choice_quality->value() + 1) / 10.0f;
    } catch (const std::exception& e) {
        LOG_ERROR_STREAM << "Error parsing arguments: " << e.what();
        std::string err = "Error: ";
        err += e.what();
        info->value(err.c_str());
        goto FAILED;
    } catch (...) {
        info->value("Invalid Arguments!");
        goto FAILED;
    }

    pages::setup();

    return true;

FAILED:
    info->textcolor(err_color);
    button_start->activate();
    return false;
}

int GUI::run() {

#ifdef PLATFORM_WINDOWS
#include "assets/favicon.xpm"
    Fl_Pixmap ico(favicon);
    Fl_RGB_Image img(&ico);
    window->icon(&img); 
#endif

    window->end();
    window->show();
    return Fl::run();
}
