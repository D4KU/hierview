module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <nfd.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

export module window;

import <iostream>; // cerr
import <filesystem>;
import <string>;
import <vector>;
import <unordered_map>;
import <chrono>;
import <thread>;

import app;
import cache;


namespace hierview
{
namespace fs = std::filesystem;
using FrameMap = std::unordered_map<int, std::vector<fs::directory_entry>>;


void on_size_change(GLFWwindow* window, int width, int height)
{
    app::width = width;
    app::height = height;
}


std::vector<fs::path> split_path(const fs::path& path)
{
    std::vector<fs::path> out;
    for (const auto& p : path)
        out.push_back(p);
    return out;
}


fs::path join_path(const std::vector<fs::path>& parts)
{
    fs::path out;
    for (const auto& p : parts)
        out /= p;
    return out;
}


fs::path replace_part(const fs::path& path, const fs::path& part, int i)
{
    auto parts = split_path(path);
    parts.at(i) = part;
    fs::path out = join_path(parts);
    out.replace_extension(path.extension());
    return out;
}


std::string inc_index(const std::string& str, int step = 1, int group = -1)
{
    // Stores {start, length} of numbers
    std::vector<std::pair<size_t, size_t>> groups;

    // Find numeric groups
    {
        std::regex pat("(\\d+)");
        auto it = std::sregex_iterator(str.begin(), str.end(), pat);
        auto end = std::sregex_iterator();
        for (; it != end; ++it)
            groups.push_back({it->position(), it->length()});
    }

    // No numbers found in the string
    if (groups.empty())
        return str;

    // Determine which number to modify
    if (group < 0)
        group += groups.size();

    auto& [pos, len] = groups.at(group);
    std::string num_str = str.substr(pos, len);
    std::string num_str_new = std::to_string(std::stoi(num_str) + step);

    // Convert back with zero-padding
    if (num_str_new.length() < num_str.length())
        num_str_new.insert(0, num_str.length() - num_str_new.length(), '0');

    // Replace the original number in the string
    return str.substr(0, pos) + num_str_new + str.substr(pos + len);
}


void drag_to_scroll(const ImVec2& delta, ImGuiMouseButton btn)
{
    ImGuiWindow* win = ImGui::GetCurrentContext()->CurrentWindow;
    ImGui::ButtonBehavior(
        win->Rect(), win->GetID("##scrolldraggingoverlay"), NULL, NULL, btn);
    ImGui::SetScrollX(win, win->Scroll.x - delta.x);
    ImGui::SetScrollY(win, win->Scroll.y - delta.y);
}

std::string choose_file()
{
    clear_cache();
    NFD::Guard guard;
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{"Images", "png,jpg,jpeg"}};
    if (NFD::OpenDialog(path, filters, 1) != NFD_OKAY)
        return "";
    return path.get();
}


export class Window
{
public:
    int draw(const fs::path& path_in)
    {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* window = glfwCreateWindow(
            app::width, app::height, "Hierarchy View", NULL, NULL);
        if (window == nullptr)
        {
            std::cerr << "Failed to create window" << std::endl;
            glfwTerminate();
            return -1;
        }

        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, on_size_change);

        // Load OpenGL function pointers
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return -1;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        // Second arg install_callback:
        // install GLFW callbacks and chain to existing ones?
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard
                       |  ImGuiConfigFlags_NavEnableGamepad
                       |  ImGuiConfigFlags_DockingEnable
                       ;

        if (!path_in.empty())
            load_img(fs::absolute(path_in));

        double fps_r = 1.0 / app::fps;
        double t_last = glfwGetTime();

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
            // ImGui::ShowDemoWindow();

            draw_path_win();
            draw_img_win((ImTextureID)(intptr_t)tex);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
            double t_diff = t_last + fps_r - glfwGetTime();
            if (t_diff > 0)
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int>(1000 * t_diff)));
            t_last = glfwGetTime();
        }

        glfwTerminate();
        return 0;
    }

private:
    void load_img(const fs::path& path_in)
    {
        int w, h;
        unsigned char* data = stbi_load(path_in.string().c_str(), &w, &h, NULL, 4);

        if (!data)
        {
            std::cerr << "Failed to load image from " << path_in << std::endl;
            return;
        }

        if (tex && w == width && h == height)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        else
        {
            glDeleteTextures(1, &tex);
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            width = w;
            height = h;
        }

        stbi_image_free(data);
        indices.clear();
        for (const fs::path& part : path_in)
            indices.push_back(last_index(part.string()));

        path_d = path_in;
    }

    void draw_path_part(const fs::path& part, int i, int& i_dirty, int& step)
    {
        std::string i_str = std::to_string(i);

        ImGui::Text(part.string().c_str());
        ImGui::SameLine();
        if (indices.at(i) < 0)
            return;

        if (ImGui::SmallButton(("v##v" + i_str).c_str()))
        {
            step = -1;
            i_dirty = i;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(("^##^" + i_str).c_str()))
        {
            step = 1;
            i_dirty = i;
        }
    }

    bool adjust_path(const std::vector<fs::path>& parts, int start, fs::path& path)
    {
        for (int i = start; i < indices.size(); i++)
        {
            if (indices[i] < 0)
            {
                path /= parts[i].filename();
                if (!fs::exists(path))
                    return false;
            }
            else
            {
                auto& entries = get_entries(path);
                auto it = entries.find(indices[i]);
                if (it == entries.end())
                    return false;

                bool no_entry = true;
                for (const auto& entry : it->second)
                {
                    if (i == indices.size() - 1)
                    {
                        if (!entry.is_regular_file())
                            continue;
                    }
                    else
                    {
                        if (!entry.is_directory())
                            continue;
                    }
                    path = entry.path();
                    no_entry = false;
                    break;
                }

                if (no_entry)
                    return false;
            }
        }
        return true;
    }

    void draw_path_win()
    {
        ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Path") || path_d.empty())
        {
            ImGui::End();
            return;
        }

        int i_dirty = -1;
        int step = 0;
        int i = 0;
        for (const fs::path& part : path_d.parent_path())
        {
            if (part.has_stem())
            {
                draw_path_part(part, i, i_dirty, step);
                ImGui::SameLine();
                ImGui::Text("/");
            }
            else if (part.has_root_name())
            {
                ImGui::Text(part.root_name().string().c_str());
                ImGui::SameLine();
                ImGui::Text("/");
            }
            ImGui::SameLine();
            i++;
        }

        draw_path_part(path_d.stem(), i, i_dirty, step);
        if (i_dirty >= 0)
        {
            indices[i_dirty] += step;
            auto parts = split_path(path_d);
            fs::path parent;
            for (i = 0; i < i_dirty; i++)
                parent /= parts[i];
            if (adjust_path(parts, i_dirty, parent))
                load_img(parent);
            else
                indices[i_dirty] -= step;
        }

        ImGui::SameLine();
        ImGui::Text(path_d.extension().string().c_str());
        ImGui::End();
    }

    void draw_img_win(ImTextureID id)
    {
        ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(
            "Image",
            NULL,
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::End();
            return;
        }

        if (!ImGui::BeginMenuBar())
        {
            ImGui::EndMenuBar();
            return;
        }

        std::string path_in = "";
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O))
            path_in = choose_file();

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O"))
                path_in = choose_file();
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();

        if (!path_in.empty())
            load_img(path_in);

        auto& io = ImGui::GetIO();
        if (ImGui::IsWindowHovered())
        {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                drag_to_scroll(io.MouseDelta, ImGuiButtonFlags_MouseButtonLeft);
            else if (!ImGui::IsAnyItemActive())
                zoom += io.MouseWheel * app::scroll_sensitivity;
        }

        auto& pad = ImGui::GetStyle().WindowPadding;
        float w_new = (ImGui::GetContentRegionAvail().x - pad.x) * zoom;
        ImGui::Image(id, ImVec2(w_new, w_new * height / width));
        ImGui::End();
    }

    float zoom = 1;
    int width = 0;
    int height = 0;
    unsigned int tex = 0;
    fs::path path_d;
    std::vector<int> indices;
};
}
