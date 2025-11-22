
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <png.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "json-develop/single_include/nlohmann/json.hpp"


using json = nlohmann::json;

void save_png(const char *path, int w, int h, uint8_t *rgba)
{
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info = png_create_info_struct(png);
	FILE *fp = fopen(path, "wb");

	if (!fp) return;
	if (setjmp(png_jmpbuf(png))) { fclose(fp); return; }

	png_init_io(png, fp);
	png_set_IHDR(png, info, w, h, 8,
		PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png, info);

	png_bytep *rows = (png_bytep *)malloc(sizeof(png_bytep) * h);
	for(int y=0;y<h;y++){
		rows[y] = rgba + y * w * 4;
	}

	png_write_image(png, rows);
	png_write_end(png, NULL);

	free(rows);

	fclose(fp);
	png_destroy_write_struct(&png, &info);
}

static GLuint compile_program_from_strings(const char* vert_src, const char* frag_src, std::string &errstr){

	static char buf[8192];

	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, &vert_src, NULL);
	glCompileShader(vert);
	GLint ok = 0; glGetShaderiv(vert, GL_COMPILE_STATUS, &ok);
	if(!ok){
		glGetShaderInfoLog(vert, sizeof(buf), NULL, buf);
		errstr = std::string("VERTEX: ") + buf;
		glDeleteShader(vert);
		return 0;
	}

	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, &frag_src, NULL);
	glCompileShader(frag);
	glGetShaderiv(frag, GL_COMPILE_STATUS, &ok);
	if(!ok){
		glGetShaderInfoLog(frag, sizeof(buf), NULL, buf);
		errstr = std::string("FRAGMENT: ") + buf;
		glDeleteShader(vert);
		glDeleteShader(frag);
		return 0;
	}

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if(!ok){
		glGetProgramInfoLog(prog, sizeof(buf), NULL, buf);
		errstr = std::string("LINK: ") + buf;
		glDeleteProgram(prog);
		glDeleteShader(vert);
		glDeleteShader(frag);
		return 0;
	}

	glDetachShader(prog, vert);
	glDetachShader(prog, frag);
	glDeleteShader(vert);
	glDeleteShader(frag);
	return prog;
}

static const char *default_vert = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 uv;
void main(){
	uv = aPos * 0.5 + 0.5;
	gl_Position = vec4(aPos, 0.0, 1.0);
}
)glsl";

static const char *_default_frag_template = R"glsl(
#version 330 core
out vec4 FragColor;
in vec2 uv;

uniform vec2 iResolution;
uniform float iTime;
// uniform vec4 iMouse; // xy: current, zw: click

void mainImage(out vec4 fragColor, in vec2 fragCoord);

void main(){
	vec2 fragCoord = uv * iResolution;
	vec4 color = vec4(0.0);
	mainImage(color, fragCoord);
	FragColor = color;
}
)glsl";

static const char *default_frag_template = R"glsl(
void mainImage(out vec4 fragColor, in vec2 fragCoord){
	vec2 p = (fragCoord - 0.5 * iResolution) / iResolution.y;
	float t = iTime;
	float d = length(p);
	float v = 0.5 + 0.5 * cos(10.0 * d - t * 5.0);
	fragColor = vec4(vec3(v), 1.0);
}
)glsl";

typedef enum : int {
	UNIFORM_TYPE_NONE = 0,
	UNIFORM_TYPE_F1,
	UNIFORM_TYPE_F2,
	UNIFORM_TYPE_F3,
	UNIFORM_TYPE_F4,
	UNIFORM_TYPE_COLOR,
	UNIFORM_TYPE_MAX_NUMBER
} UniformType;

static const char * const UniformTypeList[UNIFORM_TYPE_MAX_NUMBER] = {
	"none",
	"float",
	"float2",
	"float3",
	"float4",
	"color"
};

typedef struct _UniformValue {
	bool is_setting;
	int type;
	union {
		struct {
			float v;
			float min;
			float max;
		} f1;
		struct {
			float v[2];
			float min[2];
			float max[2];
		} f2;
		struct {
			float v[3];
			float min[3];
			float max[3];
		} f3;
		struct {
			float v[4];
			float min[4];
			float max[4];
		} f4;
		struct {
			float v[3];
			float min[3];
			float max[3];
		} color;
	};
} UniformValue;

#include <map>
#include <math.h>

static std::chrono::_V2::system_clock::time_point prev_now;

int program_main(void){

	int res;

	if(!glfwInit()){
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	do {
		GLFWwindow *window = glfwCreateWindow(1280, 720, "LiveShader", NULL, NULL);
		if(window == NULL){
			res = -1;
			break;
		}

		glfwMakeContextCurrent(window);
		glfwSwapInterval(1);

		do {
			if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
				fprintf(stderr,"glad failed\n");
				res = -1;
				break;
			}

			extern unsigned char g_mainFontJp[];
			extern int g_mainFontJp_Len;

			// ImGui init
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.LogFilename = NULL;
			io.IniFilename = NULL;
			io.FontGlobalScale = 1.25f;
			ImFontConfig config;
			config.MergeMode = true;
			io.Fonts->AddFontDefault();
			// io.Fonts->AddFontFromFileTTF("./NotoSansJP-Medium.ttf", 18.0f, &config, io.Fonts->GetGlyphRangesJapanese());
			io.Fonts->AddFontFromMemoryTTF(g_mainFontJp, g_mainFontJp_Len, 18.0f, &config, io.Fonts->GetGlyphRangesJapanese());

			ImGui::StyleColorsDark();
			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 330");
		/*
			if(0){
				glDisable(GL_BLEND);
			}else{
				glEnable(GL_BLEND);
				// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glBlendFunc(GL_SRC_ALPHA, GL_ZERO);
			}
		*/

			// Fullscreen quad (NDC)
			static const float quadVerts[] = {
				-1.0f, -1.0f,
				 1.0f, -1.0f,
				-1.0f,  1.0f,
				 1.0f,  1.0f
			};
			GLuint vao, vbo;
			glGenVertexArrays(1, &vao);
			glGenBuffers(1, &vbo);
			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
			glBindVertexArray(0);

			GLuint fbo = 0, colorTex = 0;
			int fbWidth = 1024, fbHeight = 1024;
			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);

			glGenTextures(1, &colorTex);
			glBindTexture(GL_TEXTURE_2D, colorTex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fbWidth, fbHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
			// glEnable(GL_FRAMEBUFFER_SRGB);

			if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
				fprintf(stderr, "FBO incomplete!\n");
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// shader program
			std::string frag_text(default_frag_template);
			std::string compile_err;
			GLuint program = compile_program_from_strings(default_vert, (std::string(_default_frag_template) + frag_text).c_str(), compile_err);
			if(!program){
				fprintf(stderr, "Initial compile failed: %s\n", compile_err.c_str());
			}

			auto tstart = std::chrono::high_resolution_clock::now();

			bool compile_requested = false;
			bool need_recompile = false;
			static char shader_buf[1<<16];
			memset(shader_buf, 0, sizeof(shader_buf));
			strncpy(shader_buf, frag_text.c_str(), sizeof(shader_buf)-1);

			// mouse tracking
			double mouse_x=0, mouse_y=0;
			bool mouse_down=false;
			double click_x=0, click_y=0;

			prev_now = std::chrono::high_resolution_clock::now();

			while(!glfwWindowShouldClose(window)){
				glfwPollEvents();

				// mouse state
				int mx, my;
				int mb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
				double xpos, ypos;
				glfwGetCursorPos(window, &xpos, &ypos);
				glfwGetWindowSize(window, &mx, &my);
				mouse_x = xpos; mouse_y = ypos;
				if(mb==GLFW_PRESS && !mouse_down){
					mouse_down = true;
					click_x = xpos; click_y = ypos;
				}else if(mb==GLFW_RELEASE){
					mouse_down = false;
				}

				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();

				static float time_param = 0.0f;
				static bool is_stopped = false;

				auto now = std::chrono::high_resolution_clock::now();
				if(is_stopped == false){
					std::chrono::duration<float> _elapsed = now - prev_now;
					time_param += _elapsed.count();
				}
				prev_now = now;

				static char g_uniform_name[0x40];
				static int current_uniform_type = 0;
				static std::map<std::string, UniformValue> g_uniform_list;
				static float time_scale = 1.0f;

				{ // Shader Preview
					ImGui::SetNextWindowSize(ImVec2(405, 520), ImGuiCond_FirstUseEver);
					ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
					ImGui::Begin("Shader Preview");

					if(is_stopped){
						if(ImGui::Button("Play")){
							is_stopped = false;
						}
					}else{
						if(ImGui::Button("Stop")){
							is_stopped = true;
						}
					}
					ImGui::SameLine();

					ImGui::Text("| Time: %.07f (%.07f * %.07f)", time_param * time_scale, time_param, time_scale);

					ImGui::SliderFloat("Time scale", &time_scale, 0.5f, 2.0f, "%.07f");

					if(ImGui::Button("Save")){

						void *buffer = malloc(fbWidth * fbHeight * sizeof(uint32_t) + 0x3FF);
						void *buffer_align = (void *)(((uintptr_t)buffer + 0x3FF) & ~0x3FF);


						// glBindFramebuffer(GL_FRAMEBUFFER, fbo);
						// glReadBuffer(GL_COLOR_ATTACHMENT0);
						// glFinish();
						// glReadPixels(0, 0, fbWidth, fbHeight, GL_RGBA, GL_UNSIGNED_BYTE, buffer_align);
						// glBindFramebuffer(GL_FRAMEBUFFER, 0);

						glBindTexture(GL_TEXTURE_2D, colorTex);
						glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer_align);
						glBindTexture(GL_TEXTURE_2D, 0);

						save_png("./result.png", fbWidth, fbHeight, (uint8_t *)buffer_align);
						// png_save_image("./result.png", buffer_align, fbWidth, fbHeight, fbWidth);
						free(buffer);
					}

					ImGui::Text("Shader output:");

					ImVec2 size = ImGui::GetContentRegionAvail();
					size.y -= 3.0f;

					ImGui::Image(
						(ImTextureID)(uintptr_t)colorTex,
						ImVec2(fmax(384.0f, size.y), fmax(384.0f, size.y)),
						ImVec2(0, 0), ImVec2(1, 1)
					);

					ImGui::End();
				}

				ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowPos(ImVec2(500, 50), ImGuiCond_FirstUseEver);
				ImGui::Begin("Shader Editor");

				if(ImGui::BeginTabBar("##tabbar"), ImGuiTabBarFlags_::ImGuiTabBarFlags_NoTooltip){

					if(ImGui::BeginTabItem(("Code Editor"))){

						if(compile_requested || need_recompile){
							// compile
							std::string err;
							GLuint newprog = compile_program_from_strings(default_vert, (std::string(_default_frag_template) + shader_buf).c_str(), err);
							if(newprog){
								if(program){
									glDeleteProgram(program);
								}
								program = newprog;
								compile_err = "OK";
							}else{
								compile_err = err;
							}
							compile_requested = false;
							need_recompile = false;
						}

						if(ImGui::CollapsingHeader("Compile log")){
							ImGui::BeginChild("log", ImVec2(0,100), true);
							ImGui::TextWrapped("%s", compile_err.c_str());
							ImGui::EndChild();
						}

						if(ImGui::Button("Compile")){
							compile_requested = true;
						}

						ImGui::SameLine();
						if(ImGui::Button("Reset")){
							strncpy(shader_buf, default_frag_template, sizeof(shader_buf)-1);
							need_recompile = true;
						}

						ImGui::Separator();
						ImGui::Text("fragment shader code:");

						ImVec2 size = ImGui::GetContentRegionAvail();
						size.y -= 3.0f;

						ImGui::InputTextMultiline("##shader", shader_buf, sizeof(shader_buf), ImVec2(-FLT_MIN, fmax(ImGui::GetTextLineHeight()*20, size.y)), ImGuiInputTextFlags_AllowTabInput);
						ImGui::EndTabItem();
					}

					if(ImGui::BeginTabItem(("Uniform Editor"))){
						if(ImGui::CollapsingHeader("Uniform import/export")){

							static bool enable_overwrite = false;
							static char uniform_ie_text[0x40000];

							if(ImGui::Button("Import")){
								try {
									json import_data = json::parse(uniform_ie_text);
									if(import_data.end() == import_data.find("list")){
										throw std::runtime_error("error-1");
									}
									json& list = import_data["list"];
									if(list.is_array() == false){
										throw std::runtime_error("error-2");
									}
									for(auto& v : list){
										if(v.is_object() == false){
											throw std::runtime_error("error-3");
										}
										if(v.end() == v.find("name")){
											throw std::runtime_error("error-4");
										}
										if(v.end() == v.find("type")){
											throw std::runtime_error("error-5");
										}
										if(v.end() == v.find("uniform")){
											throw std::runtime_error("error-6");
										}
										if(v.end() == v.find("uniform_min")){
											throw std::runtime_error("error-7");
										}
										if(v.end() == v.find("uniform_max")){
											throw std::runtime_error("error-8");
										}

										if(enable_overwrite == false && g_uniform_list.end() != g_uniform_list.find(v["name"].get<std::string>())){
											printf("skip adding uniform %s\n", v["name"].get<std::string>().c_str());
											continue;
										}

										printf("adding uniform %s\n", v["name"].get<std::string>().c_str());

										UniformValue uv;
										memset(&uv, 0, sizeof(uv));
										uv.is_setting = false;
										uv.type = v["type"].get<int>();

										auto& uniform = v["uniform"];

										switch(uv.type){
										case UNIFORM_TYPE_F1:
											uv.f1.v = uniform[0];
											uv.f1.min = v["uniform_min"][0];
											uv.f1.max = v["uniform_max"][0];
											break;
										case UNIFORM_TYPE_F2:
											for(int i=0;i<2;i++){
												uv.f2.v[i]   = uniform[i];
												uv.f2.min[i] = v["uniform_min"][i];
												uv.f2.max[i] = v["uniform_max"][i];
											}
											break;
										case UNIFORM_TYPE_F3:
											for(int i=0;i<3;i++){
												uv.f3.v[i]   = uniform[i];
												uv.f3.min[i] = v["uniform_min"][i];
												uv.f3.max[i] = v["uniform_max"][i];
											}
											break;
										case UNIFORM_TYPE_F4:
											for(int i=0;i<4;i++){
												uv.f4.v[i]   = uniform[i];
												uv.f4.min[i] = v["uniform_min"][i];
												uv.f4.max[i] = v["uniform_max"][i];
											}
											break;
										case UNIFORM_TYPE_COLOR:
											for(int i=0;i<3;i++){
												uv.color.v[i]   = uniform[i];
												uv.color.min[i] = 0.0f;
												uv.color.max[i] = 1.0f;
											}
											break;
										default:
											break;
										}

										g_uniform_list[v["name"].get<std::string>()] = uv;
									}
								} catch(std::runtime_error &err){
								}
							}
							ImGui::SameLine();
							if(ImGui::Button("Export")){
								json export_data;

								json list = json::array();

								for(auto& v : g_uniform_list){
									UniformValue& uv = v.second;

									json item;
									item["name"] = v.first;
									item["type"] = uv.type;

									json uniform_list = json::array();
									json uniform_min = json::array();
									json uniform_max = json::array();

									switch(uv.type){
									case UNIFORM_TYPE_F1:
										uniform_list.push_back(uv.f1.v);
										uniform_min.push_back(uv.f1.min);
										uniform_max.push_back(uv.f1.max);
										break;
									case UNIFORM_TYPE_F2:
										for(int i=0;i<2;i++){
											uniform_list.push_back(uv.f2.v[i]);
											uniform_min.push_back(uv.f2.min[i]);
											uniform_max.push_back(uv.f2.max[i]);
										}
										break;
									case UNIFORM_TYPE_F3:
										for(int i=0;i<3;i++){
											uniform_list.push_back(uv.f3.v[i]);
											uniform_min.push_back(uv.f3.min[i]);
											uniform_max.push_back(uv.f3.max[i]);
										}
										break;
									case UNIFORM_TYPE_F4:
										for(int i=0;i<4;i++){
											uniform_list.push_back(uv.f4.v[i]);
											uniform_min.push_back(uv.f4.min[i]);
											uniform_max.push_back(uv.f4.max[i]);
										}
										break;
									case UNIFORM_TYPE_COLOR:
										for(int i=0;i<3;i++){
											uniform_list.push_back(uv.color.v[i]);
											uniform_min.push_back(uv.color.min[i]);
											uniform_max.push_back(uv.color.max[i]);
										}
										break;
									default:
										break;
									}

									item["uniform"] = uniform_list;
									item["uniform_min"] = uniform_min;
									item["uniform_max"] = uniform_max;
									list.push_back(item);
								}

								export_data["list"] = list;
								strncpy(uniform_ie_text, export_data.dump().c_str(), sizeof(uniform_ie_text) - 1);
								uniform_ie_text[sizeof(uniform_ie_text) - 1] = 0;
							}

							ImGui::Checkbox("Enable Uniform Overwrite", &enable_overwrite);

							ImGui::InputTextMultiline("##uniform_ie_text", uniform_ie_text, sizeof(uniform_ie_text), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight()*7), ImGuiInputTextFlags_AllowTabInput);
						}

						ImGui::InputText("##g_uniform_name", g_uniform_name, sizeof(g_uniform_name));
						ImGui::Combo("Uniform Type", &current_uniform_type, UniformTypeList, UNIFORM_TYPE_MAX_NUMBER);

						if(ImGui::Button("Add Uniform") && strlen(g_uniform_name) != 0 && g_uniform_list.end() == g_uniform_list.find(g_uniform_name) && current_uniform_type != UNIFORM_TYPE_NONE){

							UniformValue uv;
							memset(&uv, 0, sizeof(uv));
							uv.is_setting = false;
							uv.type = current_uniform_type;

							switch(uv.type){
							case UNIFORM_TYPE_F1:
								uv.f1.min = 0.0f;
								uv.f1.max = 10.0f;
								break;
							case UNIFORM_TYPE_F2:
								for(int i=0;i<2;i++){
									uv.f2.min[i] = 0.0f;
									uv.f2.max[i] = 10.0f;
								}
							case UNIFORM_TYPE_F3:
								for(int i=0;i<3;i++){
									uv.f3.min[i] = 0.0f;
									uv.f3.max[i] = 10.0f;
								}
								break;
							case UNIFORM_TYPE_F4:
								for(int i=0;i<4;i++){
									uv.f4.min[i] = 0.0f;
									uv.f4.max[i] = 10.0f;
								}
								break;
							case UNIFORM_TYPE_COLOR:
								for(int i=0;i<3;i++){
									uv.color.min[i] = 0.0f;
									uv.color.max[i] = 1.0f;
								}
								break;
							default:
								break;
							}

							g_uniform_list[g_uniform_name] = uv;
						}

						std::string todo_remove;

						ImVec2 size = ImGui::GetContentRegionAvail();

						ImGui::BeginChild("##uniform_list", ImVec2(0, fmax(384.0f, size.y - 3.0f)), true);
						for(auto& v : g_uniform_list){
							UniformValue& uv = v.second;
							switch(uv.type){
							case UNIFORM_TYPE_F1:
								{
									ImGui::BeginChild(v.first.c_str(), ImVec2(0, 110), true);
									ImGui::Text("%s", v.first.c_str());
									ImGui::SliderFloat("##slider0", &(uv.f1.v), uv.f1.min, uv.f1.max, "%.07f");
									float min_max[2];
									min_max[0] = uv.f1.min;
									min_max[1] = uv.f1.max;
									ImGui::InputFloat2("min/max", min_max);
									uv.f1.min = min_max[0];
									uv.f1.max = min_max[1];
									bool used = uv.is_setting;
									ImGui::Checkbox("##used", &used);
									ImGui::SameLine();
									if(ImGui::Button("Remove")){
										todo_remove = v.first;
									}
									ImGui::EndChild();
								}
								break;
							case UNIFORM_TYPE_F2:
								{
									ImGui::BeginChild(v.first.c_str(), ImVec2(0, 165), true);
									ImGui::Text("%s", v.first.c_str());
									ImGui::SliderFloat("##slider0", &(uv.f2.v[0]), uv.f2.min[0], uv.f2.max[0], "%.07f");
									ImGui::SliderFloat("##slider1", &(uv.f2.v[1]), uv.f2.min[1], uv.f2.max[1], "%.07f");
									ImGui::InputFloat2("min", uv.f2.min);
									ImGui::InputFloat2("max", uv.f2.max);

									bool used = uv.is_setting;
									ImGui::Checkbox("##used", &used);
									ImGui::SameLine();
									if(ImGui::Button("Remove")){
										todo_remove = v.first;
									}
									ImGui::EndChild();
								}
								break;
							case UNIFORM_TYPE_F3:
								{
									ImGui::BeginChild(v.first.c_str(), ImVec2(0, 190), true);
									ImGui::Text("%s", v.first.c_str());
									ImGui::SliderFloat("##slider0", &(uv.f3.v[0]), uv.f3.min[0], uv.f3.max[0], "%.07f");
									ImGui::SliderFloat("##slider1", &(uv.f3.v[1]), uv.f3.min[1], uv.f3.max[1], "%.07f");
									ImGui::SliderFloat("##slider2", &(uv.f3.v[2]), uv.f3.min[2], uv.f3.max[2], "%.07f");
									ImGui::InputFloat3("min", uv.f3.min);
									ImGui::InputFloat3("max", uv.f3.max);
									bool used = uv.is_setting;
									ImGui::Checkbox("##used", &used);
									ImGui::SameLine();
									if(ImGui::Button("Remove")){
										todo_remove = v.first;
									}
									ImGui::EndChild();
								}
								break;
							case UNIFORM_TYPE_F4:
								{
									ImGui::BeginChild(v.first.c_str(), ImVec2(0, 220), true);
									ImGui::Text("%s", v.first.c_str());
									ImGui::SliderFloat("##slider0", &(uv.f4.v[0]), uv.f4.min[0], uv.f4.max[0], "%.07f");
									ImGui::SliderFloat("##slider1", &(uv.f4.v[1]), uv.f4.min[1], uv.f4.max[1], "%.07f");
									ImGui::SliderFloat("##slider2", &(uv.f4.v[2]), uv.f4.min[2], uv.f4.max[2], "%.07f");
									ImGui::SliderFloat("##slider3", &(uv.f4.v[3]), uv.f4.min[3], uv.f4.max[3], "%.07f");
									ImGui::InputFloat4("min", uv.f4.min);
									ImGui::InputFloat4("max", uv.f4.max);
									bool used = uv.is_setting;
									ImGui::Checkbox("##used", &used);
									ImGui::SameLine();
									if(ImGui::Button("Remove")){
										todo_remove = v.first;
									}
									ImGui::EndChild();
								}
								break;
							case UNIFORM_TYPE_COLOR:
								{
									ImGui::BeginChild(v.first.c_str(), ImVec2(0, 90), true);
									ImGui::Text("%s", v.first.c_str());
									ImGui::ColorEdit3("##slider0", uv.color.v);
									bool used = uv.is_setting;
									ImGui::Checkbox("##used", &used);
									ImGui::SameLine();
									if(ImGui::Button("Remove")){
										todo_remove = v.first;
									}
									ImGui::EndChild();
								}
								break;
							default:
								break;
							}
						}
						ImGui::EndChild();

						if(todo_remove.empty() == false){
							g_uniform_list.erase(todo_remove);
						}

						ImGui::EndTabItem();
					}
					ImGui::EndTabBar();
				}

				ImGui::End();

				// Render
				int win_w, win_h;
				glfwGetFramebufferSize(window, &win_w, &win_h);
				glViewport(0, 0, win_w, win_h);
				glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);

				{
					glBindFramebuffer(GL_FRAMEBUFFER, colorTex);
					glViewport(0, 0, fbWidth, fbHeight);
					// glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
					// glClear(GL_COLOR_BUFFER_BIT);

					if(program){
						glUseProgram(program);

						int resLoc = glGetUniformLocation(program, "iResolution");
						if(resLoc >= 0){
							glUniform2f(resLoc, (float)fbWidth, (float)fbHeight);
						}

						int timeLoc = glGetUniformLocation(program, "iTime");
						if(timeLoc >= 0){
							glUniform1f(timeLoc, time_param * time_scale);
						}

						for(auto& v : g_uniform_list){
							UniformValue& uv = v.second;

							int uniLoc = glGetUniformLocation(program, v.first.c_str());
							if(uniLoc < 0){
								uv.is_setting = false;
								continue;
							}

							uv.is_setting = true;

							switch(uv.type){
							case UNIFORM_TYPE_F1:
								glUniform1f(uniLoc, uv.f1.v);
								break;
							case UNIFORM_TYPE_F2:
								glUniform2f(uniLoc, uv.f2.v[0], uv.f2.v[1]);
								break;
							case UNIFORM_TYPE_F3:
								glUniform3f(uniLoc, uv.f3.v[0], uv.f3.v[1], uv.f3.v[2]);
								break;
							case UNIFORM_TYPE_F4:
								glUniform4f(uniLoc, uv.f4.v[0], uv.f4.v[1], uv.f4.v[2], uv.f4.v[3]);
								break;
							case UNIFORM_TYPE_COLOR:
								glUniform3f(uniLoc, uv.color.v[0], uv.color.v[1], uv.color.v[2]);
								break;
							default:
								break;
							}
						}

						// draw quad
						glBindVertexArray(vao);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
						glBindVertexArray(0);
						glUseProgram(0);
					}

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				}

				// Draw ImGui on top
				ImGui::Render();
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

				glfwSwapBuffers(window);
			}

			// cleanup
			if(program){
				glDeleteProgram(program);
			}
			glDeleteBuffers(1, &vbo);
			glDeleteVertexArrays(1, &vao);

			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		} while(0);

		glfwDestroyWindow(window);
	} while(0);

	glfwTerminate();

	return res;
}

int main(){

	int res = program_main();
	if(res < 0){
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
