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
import <stack>;

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
        {
            load_img(fs::absolute(path_in));
            update_indices();
        }

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
            ImGui::ShowDemoWindow();

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
        parts.clear();
        for (const fs::path& part : path_in)
            parts.push_back(part);
    }

    void draw_path_part(int i, int& i_dirty, int& j_new, fs::directory_entry& entry_new)
    {
        if (indices.at(i) < 0)
        {
            ImGui::Text(parts[i].string().c_str());
            return;
        }

        static int i_up = -1;
        std::string i_str = std::to_string(i);
        auto id_popup = ("popup" + i_str).c_str();

        if (ImGui::Button(parts[i].string().c_str()))
        {
            ImGui::OpenPopup(id_popup);
            i_up = i;
        }

        if (i == i_up && ImGui::BeginPopup(id_popup))
        {
            fs::path parent;
            for (int j = 0; j < i; j++)
                parent /= parts[j];

            bool last_i = i == parts.size() - 1;
            const auto& entries = get_entries(parent);

            for (const auto& [j, hits] : entries)
            {
                for (const auto& entry : hits)
                {
                    if (last_i && entry.is_directory())
                        continue;
                    if (ImGui::Selectable(entry.path().filename().string().c_str()))
                    {
                        i_dirty = i;
                        j_new = j;
                        entry_new = entry;
                    }
                }
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(("v##v" + i_str).c_str()))
        {
            i_dirty = i;
            j_new = indices[i] - 1;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(("^##^" + i_str).c_str()))
        {
            i_dirty = i;
            j_new = indices[i] + 1;
        }
    }

    int path_size(const fs::path& p)
    {
        return std::distance(p.begin(), p.end());
    }

    bool complete_path(fs::directory_entry& start)
    {
        std::stack<std::pair<int, fs::directory_entry>> stack;
        int i_start = path_size(start.path());
        stack.emplace(i_start, start);
        bool fell_back = false;

        while (!stack.empty())
        {
            auto [i, entry] = stack.top();
            stack.pop();

            if (entry.is_directory())
            {
                if (i >= indices.size())
                    continue;
            }
            else
            {
                start = entry;
                if (i >= indices.size())
                    return true;
                fell_back = true;
                continue;
            }

            if (indices[i] < 0)
            {
                fs::path child = entry.path() / parts[i];
                if (fs::exists(child))
                    stack.emplace(i + 1, child);
                continue;
            }

            const auto& childs = get_entries(entry.path());
            auto it = childs.find(indices[i]);
            if (it == childs.end())
                continue;
            for (const auto& child : it->second)
                stack.emplace(i + 1, child);
        }

        if (!fell_back)
            return false;

        indices.resize(path_size(start));
        return true;
    }

    void draw_path_win()
    {
        ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Path") || parts.empty())
        {
            ImGui::End();
            return;
        }

        int i_dirty = -1;
        int j_new = -1;
        fs::directory_entry entry_new;

        for (int i = 0; i < parts.size() - 1; i++)
        {
            if (parts[i].has_stem())
            {
                draw_path_part(i, i_dirty, j_new, entry_new);
                ImGui::SameLine();
                ImGui::Text("/");
            }
            else if (parts[i].has_root_name())
            {
                ImGui::Text(parts[i].root_name().string().c_str());
                ImGui::SameLine();
                ImGui::Text("/");
            }
            ImGui::SameLine();
        }

        draw_path_part(parts.size() - 1, i_dirty, j_new, entry_new);
        if (i_dirty >= 0 && j_new >= 0)
        {
            int j_old = indices[i_dirty];
            indices[i_dirty] = j_new;
            bool success = true;

            if (entry_new.path().empty())
            {
                fs::path path_new;
                for (int i = 0; i < i_dirty; i++)
                    path_new /= parts[i];
                entry_new.assign(path_new);
                success = complete_path(entry_new);
            }
            else if (entry_new.is_directory())
            {
                success = complete_path(entry_new);
            }

            if (success)
                load_img(entry_new.path());
            else
                indices[i_dirty] = j_old;
        }

        ImGui::End();
    }

    void update_indices()
    {
        indices.clear();
        for (const fs::path& part : parts)
            indices.push_back(last_index(part.string()));
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
        {
            load_img(path_in);
            update_indices();
        }

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
    std::vector<fs::path> parts;
    std::vector<int> indices;
};
}
