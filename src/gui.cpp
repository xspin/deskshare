#include "logger.h"
#include "utils.h"
#include "gui.h"
#include "pages.h"

static inline const std::string compilerInfo() {
    std::stringstream ss;

#if defined(__clang__)
    // Clang
    ss << "Clang " << __clang_version__;
#elif defined(__GNUC__)
    // GCC
    ss << "GCC " << __GNUC__  << "." <<  __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
#endif

    return ss.str();
}

static const Fl_Color DEFAULT_BG_COLOR = fl_rgb_color(236, 236, 236);

static const Fl_Color DEFAULT_FONT_COLOR = fl_rgb_color(67, 67, 67);


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
    ss << " " << g_config.clients << " ☺  ";
    ss << std::fixed << std::setprecision(1) << (g_config.frames / TIMEOUT_INTERVAL) << " fps  ";
    ss << utils::speedString(g_config.bytes / TIMEOUT_INTERVAL);

    self->output_status->value(ss.str().c_str());
    g_config.frames = 0;
    g_config.bytes = 0;

    Fl::repeat_timeout(TIMEOUT_INTERVAL, onTimeout, data);
}

void GUI::onMessage(const char* msg) {
    static bool show = false;
    if (show) {
        this->about->hide();
        this->display->show();
        show = false;
        return;
    }

    this->about->label(msg);
    this->display->hide();
    this->about->show();
    show = true;

    Fl::add_timeout(10, [](void* data){
        GUI* self = static_cast<GUI*>(data);
        self->about->hide();
        self->display->show();
        show = false;
    }, this);

}

void GUI::onAbout(Fl_Widget* w, void* data) {
    static std::string about_info;
    if (about_info.empty()) {
        std::stringstream ss;
        ss << "\n\nDeskShare v" << APP_VERSION << "\n\n"
            << "Compiler: " << compilerInfo() << "\n\n"
            << "Build: " << __TIMESTAMP__ << "\n\n"
            << "Email: xnipse@@gmail.com\n\n";
        about_info = ss.str();
    }
    GUI* self = static_cast<GUI*>(data);
    self->about->align(FL_ALIGN_CENTER | FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    self->onMessage(about_info.c_str());
}

void GUI::onDetail(Fl_Widget* w, void* data) {
    static std::string details;
    GUI* self = static_cast<GUI*>(data);
    auto reqs = pages::getReqs();

    std::stringstream ss;
    ss << "\nRemote Clients:\n\n";
    auto now = std::time(nullptr);
    for (const auto& [key, ts] : reqs) {
        time_t d = now - ts;
        ss << "[" << utils::timeFmt(ts) << "]    " << key
           << "    (" << utils::gmTimeFmt(d, "%H:%M:%S") << ")\n";
    }
    details = ss.str();
    self->about->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    self->onMessage(details.c_str());
}

void GUI::init(std::function<void()> callback) {
    this->callback = callback;

    width = 500;
    height = 400;

    int w = 50;
    int h = 20;

    int dx = w + 10;
    int dy = h + 10;

    window = std::make_unique<Fl_Window>(width, height, "DeskShare " APP_VERSION);
    window->position(500, 300);

    int port_x = 20;
    int input_y = 20;
    input_port = std::make_unique<Fl_Input>(port_x, input_y, w, h, "Port");
    input_port->align(FL_ALIGN_TOP);
    input_port->value(g_config.port);
    input_port->callback(digitCheck);
    input_port->labelcolor(DEFAULT_FONT_COLOR);

    int timeout_x = port_x + dx;
    input_timeout = std::make_unique<Fl_Input>(timeout_x, input_y, w, h, "Timeout");
    input_timeout->align(FL_ALIGN_TOP);
    input_timeout->value(g_config.timeout);
    input_timeout->callback(digitCheck);
    input_timeout->labelcolor(DEFAULT_FONT_COLOR);

    int fps_x = timeout_x + dx;
    input_fps = std::make_unique<Fl_Input>(fps_x, input_y, w, h, "FPS");
    input_fps->align(FL_ALIGN_TOP);
    input_fps->value(g_config.fps);
    input_fps->callback(digitCheck, (void*)"FPS");
    input_fps->labelcolor(DEFAULT_FONT_COLOR);

    int quality_x = fps_x + dx;
    choice_quality = std::make_unique<Fl_Choice>(quality_x, input_y, w+8, h, "Quality");
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
    choice_quality->labelcolor(DEFAULT_FONT_COLOR);
    choice_quality->color(DEFAULT_BG_COLOR);

    int status_x = quality_x + dx + 10;
    int status_w = w*3 + 10;
    output_status = std::make_unique<Fl_Output>(status_x, input_y, status_w, h);
    output_status->align(FL_ALIGN_CENTER);
    output_status->value("");
    output_status->textsize(14);
    output_status->readonly(1);
    output_status->box(FL_FLAT_BOX);
    output_status->box(FL_FRAME_BOX);
    output_status->color(DEFAULT_BG_COLOR);

    Fl::add_timeout(TIMEOUT_INTERVAL, onTimeout, this);

    int start_x = status_x + status_w + 15;
    button_start = std::make_unique<Fl_Button>(start_x, input_y-5, w, h+10, "Start");
    button_start->color(DEFAULT_BG_COLOR);
    button_start->labelsize(28);
    button_start->labelfont(FL_BOLD);
    button_start->callback([](Fl_Widget* w, void* data) {
        GUI* self = static_cast<GUI*>(data);
        Fl_Button* btn = static_cast<Fl_Button*>(w);
        btn->deactivate();
        self->callback();
        self->display->take_focus();
    }, this);

    int info_x = 30;
    int info_y = input_y + dy;
    int info_w = width - info_x - 35;
    info = std::make_unique<Fl_Output>(info_x, info_y, info_w, h);
    info->readonly(1);
    // info->box(FL_FLAT_BOX);
    info->box(FL_FRAME_BOX);
    info->value("DeskShare");
    info->align(FL_ALIGN_CENTER);
    info->textsize(14);
    info->color(DEFAULT_BG_COLOR);
    info->textcolor(FL_DARK_BLUE);

    int btn_about_x = 5;
    int btn_about_y = info_y;
    button_about = std::make_unique<Fl_Button>(btn_about_x, btn_about_y, 25, h, "ⓘ"); 
    button_about->color(DEFAULT_BG_COLOR);
    button_about->callback(onAbout, this);
    button_about->labelfont(FL_BOLD);
    button_about->box(FL_FLAT_BOX);

    int btn_detail_x = info_x + info_w + 5;
    int btn_detail_y = info_y;
    button_detail  = std::make_unique<Fl_Button>(btn_detail_x, btn_detail_y, 25, h, "…");
    button_detail->color(DEFAULT_BG_COLOR);
    button_detail->labelfont(FL_BOLD);
    // button_detail->box(FL_FLAT_BOX);
    button_detail->callback(onDetail, this);
    
    int display_x = 5;
    int display_y = info_y + dy - 5;
    int display_w = width - 10;
    int display_h = height - display_y - 5;
    text_buf = std::make_unique<Fl_Text_Buffer>();
    display = std::make_unique<Fl_Text_Display>(display_x, display_y, display_w, display_h);
    display->buffer(text_buf.get());  
    display->color(FL_BLACK);       // 背景色设为黑色
    display->textcolor(FL_WHITE);   // 文字颜色设为白色
    display->textsize(14);          // 可选：设置字体大小
    display->textfont(FL_COURIER);  // 等宽字体（适合日志）
    display->take_focus();
    display->labelcolor(DEFAULT_BG_COLOR);

    int about_x = display_x + 5;
    int about_y = display_y + 5;
    int about_w = display_w - 10; 
    int about_h = display_h - 10; 
    about = std::make_unique<Fl_Box>(about_x, about_y, about_w, about_h);
    about->box(FL_FRAME_BOX);
    about->hide();
}

void GUI::activate() {
    input_port->activate();
    input_timeout->activate();
    button_start->label("▶");
    button_start->activate();
    button_start->labelcolor(FL_DARK_GREEN);
    info->value("");
}

void GUI::deactivate() {
    input_port->deactivate();
    input_timeout->deactivate();
    button_start->label("▣");
    button_start->activate();
    button_start->labelcolor(FL_RED);
}

void GUI::setInfo(const std::string& label) {
    info->value(label.c_str());
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

    window->color(DEFAULT_BG_COLOR);

    window->end();
    window->show();
    return Fl::run();
}
