#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "ColorTextureProgram.hpp"

#include "load_save_png.hpp"
#include "read_write_chunk.hpp"

#include <random>
#include <array>

PlayMode::PlayMode(Client &client_) : client(client_) {
    //initialize game
    std::vector<int> values;
    for (int i = 0; i < (rows * cols) / 2; ++i) {
        values.push_back(i); 
        values.push_back(i);
    }
    
    local_player.index = -1;
    local_player.score = 0;
    local_player.has_selected_card = false;
    local_player.selected_card_index = -1;
    
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(values.begin(), values.end(), g);

    board.resize(rows, std::vector<Card>(cols));
    int index = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            board[r][c] = {values[index++], false, false};  // Initialize card value
            //std::cout << "Initialize card (" << r << ", " << c << ") face up"  << board[r][c].face_up << "\n";
        }
    }

    glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);
    glBindVertexArray(vertex_buffer_for_color_texture_program);  // Bind the VAO

  
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);  // Bind the VBO

   
    // Position attribute
    glVertexAttribPointer(
       color_texture_program->Position_vec4,
       3,
       GL_FLOAT,
       GL_FALSE,
       sizeof(Vertex),
       (GLbyte *) 0
    );
    glEnableVertexAttribArray(color_texture_program->Position_vec4);  // position attribute

    // Color attribute
    glVertexAttribPointer(
       color_texture_program->Color_vec4,
       4,
       GL_UNSIGNED_BYTE,
       GL_TRUE,
       sizeof(Vertex),
       (GLbyte *)0 + 4*3
    );
    glEnableVertexAttribArray(color_texture_program->Color_vec4);  //color attribute

    // Texture coordinate attribute
    glVertexAttribPointer(
       color_texture_program->TexCoord_vec2,
       2,
       GL_FLOAT,
       GL_FALSE,
       sizeof(Vertex),
       (GLbyte *)0 + 4*3 + 4*1
    );
    glEnableVertexAttribArray(color_texture_program->TexCoord_vec2);  //texture coordinate attribute

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    GL_ERRORS();
    
    //card textures
    {
        std::vector< glm::u8vec4 > data;
        glm::uvec2 size(0, 0);
        load_png(data_path("backcard.png"), &size, &data, UpperLeftOrigin);
        
        std::cout << "Loaded texture size: " << size.x << "x" << size.y << std::endl;

        
        glGenTextures(1, &back_tex);
        glBindTexture(GL_TEXTURE_2D, back_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        GL_ERRORS();
    }
    
 
/*
    glGenTextures(1, &white_tex);
    glBindTexture(GL_TEXTURE_2D, white_tex);
    glm::uvec2 size = glm::uvec2(1, 1);
    std::vector<glm::u8vec4> data(size.x * size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
     */
    
    
    GL_ERRORS();
}

PlayMode::~PlayMode() {
}


// Flip a card at the specified row and column
void PlayMode::flip_card(int row, int col) {
    // Flip the card face-up
    board[row][col].face_up = true;
    std::cout << "flip card row: " << row << " col: " << col << "\n";

    if (first_flip == nullptr) {
        first_flip = &board[row][col];
    } else {
        second_flip = &board[row][col];
    }

    if (first_flip != nullptr && second_flip != nullptr) {
        if (first_flip->value == second_flip->value) {
            // If they match, mark them as matched
            first_flip->matched = true;
            second_flip->matched = true;
        } else {
            // If they don't match, flip them back down
            first_flip->face_up = false;
            second_flip->face_up = false;
        }
        
        first_flip = nullptr;
        second_flip = nullptr;
    }
}

void PlayMode::next_turn() {
    current_player = (current_player + 1) % 2;
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
    if (evt.type == SDL_MOUSEBUTTONDOWN) {
       
            /*glm::vec2 mouse_pos =  glm::vec2(
                                             (evt.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f,
                                             (evt.motion.y + 0.5f) / window_size.y *-2.0f + 1.0f
                                         );
        */
        int xMouse, yMouse;
        SDL_GetMouseState(&xMouse, &yMouse);
        //std::cout << "mouse click" << "x: " <<  xMouse << " y: " << yMouse << "\n";
        glm::vec2 mouse_pos =  glm::vec2( xMouse, yMouse);
        int row, col;
        //flip acrd
        if (get_card_from_mouse(mouse_pos, row, col,  window_size)) {
            if (board[row][col].face_up || board[row][col].matched) {
                std::cout << "Card already flipped or matched.\n";
                return false;
            }
            local_player.has_selected_card = true;
            local_player.selected_card_index = row * board[0].size() + col;
            flip_card(row, col);
            return true;
        }
    }
    return false;
}


// Update the game state
void PlayMode::update(float elapsed) {
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            std::cout << "update start card (" << r << ", " << c << ") face up"  << board[r][c].face_up << "\n";
            if(board[r][c].face_up) abort();
        }
    }
    
    // Queue data for sending to server (if needed)
    //controls.send_controls_message(&client.connection);
    if (local_player.has_selected_card) {
        uint8_t card_index_bytes[2];
        card_index_bytes[0] = local_player.selected_card_index & 0xFF;
        card_index_bytes[1] = (local_player.selected_card_index >> 8) & 0xFF;

        client.connections.back().send('c');
        client.connections.back().send(card_index_bytes[0]);
        client.connections.back().send(card_index_bytes[1]);

        local_player.has_selected_card = false;
    }
    /*
    // Reset button press counters:
    controls.left.downs = 0;
    controls.right.downs = 0;
    controls.up.downs = 0;
    controls.down.downs = 0;
    controls.jump.downs = 0;
     */

    // Send/receive data:
    client.poll([this](Connection *c, Connection::Event event){
        if (event == Connection::OnOpen) {
            std::cout << "[" << c->socket << "] opened" << std::endl;
        } else if (event == Connection::OnClose) {
            std::cout << "[" << c->socket << "] closed (!)" << std::endl;
            throw std::runtime_error("Lost connection to server!");
        } else {
            assert(event == Connection::OnRecv);
            while (c->recv_buffer.size() >= 3) {
                char type = c->recv_buffer[0];
                
                if (type == 'c') {
                    if (c->recv_buffer.size() < 3) break;

                    uint16_t card_index = (uint16_t(c->recv_buffer[2]) << 8) | uint16_t(c->recv_buffer[1]);

                    int row = card_index / cols;
                    int col = card_index % cols;

                    if (row >= 0 && row < board.size() && col >= 0 && col < board[0].size()) {
                        flip_card(row, col);
                    }

                    c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 3);
                }
                else {
                    std::cout << "Unknown message type received: " << type << std::endl;
                    c->recv_buffer.clear();
                }
            }
        }
/*
            bool handled_message;
            try {
                do {
                    handled_message = false;
                    if (game.recv_state_message(c)) handled_message = true;
                } while (handled_message);
            } catch (std::exception const &e) {
                std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
                throw e;
            }
        }
 */
    }, 0.0);
    
    // Check for game over
    bool all_matched = true;
    for (const auto &row : board) {
        for (const auto &card : row) {
            if (!card.matched) {
                all_matched = false;
                break;
            }
        }
        if (!all_matched) break;
    }

    if (all_matched) {
        game_over = true;
        std::cout << "Game Over!" << std::endl;
    }
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            std::cout << "update end card (" << r << ", " << c << ") face up"  << board[r][c].face_up << "\n";
            if(board[r][c].face_up) abort();
        }
    }
}


bool PlayMode::get_card_from_mouse(glm::vec2 const &mouse_pos, int &row, int &col, glm::uvec2 const &drawable_size) {
    glm::vec2 card_size(200.0f, 300.0f);
    int num_rows = board.size();
    int num_cols = board[0].size();
    float card_spacing_x = 40.0f;
    float card_spacing_y = 40.0f;
    float total_width = num_cols * (card_size.x + card_spacing_x) - card_spacing_x;
    float total_height = num_rows * (card_size.y + card_spacing_y) - card_spacing_y;
    
    //std::cout << "window size" << drawable_size.y << " total height " << total_height << "\n";
    
    glm::mat4 projection = glm::ortho(0.0f, float(drawable_size.x), 0.0f, float(drawable_size.y));
   
    glm::vec4 mouse_pos_vec =  glm::vec4(mouse_pos.x, mouse_pos.y, 0, 1);
    mouse_pos_vec = projection * mouse_pos_vec;
    mouse_pos_vec /= mouse_pos_vec.w;
    
    float mouse_click_x = mouse_pos_vec.x * drawable_size.x +  drawable_size.x;
    float mouse_click_y = mouse_pos_vec.y * drawable_size.y +  drawable_size.y;
    
    //std::cout << "mouse click" << "x: " <<  mouse_pos_vec.x * drawable_size.x +  drawable_size.x << " y: " << mouse_pos_vec.y * drawable_size.y + drawable_size.y << "\n";

    float start_x = (drawable_size.x * 2 - total_width) / 2.0f;
    float start_y = (drawable_size.y * 2 - total_height) / 2.0f;

    for (int r = 0; r < num_rows; ++r) {
        for (int c = 0; c < num_cols; ++c) {
            float card_x = start_x + c * (card_size.x + card_spacing_x);
            float card_y = start_y + r * (card_size.y + card_spacing_y);
            
            //std::cout << "Card x: " << card_x << " " << card_x + card_size.x << " y:" << card_y << " " << card_y + card_size.y << " row: " << r << " col: " << c << "\n";
            
            if (mouse_click_x >= card_x && mouse_click_x <= card_x + card_size.x &&
                mouse_click_y >= card_y && mouse_click_y <= card_y + card_size.y) {
                row = r;
                col = c;
                //std::cout << "flip card x: " << card_x << " y:" << card_y << " row: " << r << " col: " << c << "\n";
                
                return true;
            }
        }
    }
    return false;
}


void PlayMode::draw(glm::uvec2 const &drawable_size) {
    
    glClearColor(0.5f, 0.3f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(color_texture_program->program);

    glm::mat4 projection = glm::ortho(0.0f, float(drawable_size.x), 0.0f, float(drawable_size.y));
    glUniformMatrix4fv(color_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(projection));
    

    std::vector<Vertex> vertices;

    glm::vec2 card_size(200.0f, 300.0f);
    float card_spacing_x = 40.0f;
    float card_spacing_y = 40.0f;

    int num_rows = rows;
    int num_cols = cols;

    float total_width = num_cols * (card_size.x + card_spacing_x) - card_spacing_x;
    float total_height = num_rows * (card_size.y + card_spacing_y) - card_spacing_y;
    
    //std::cout << drawable_size.y << " total height " << total_height << "\n";

    float start_x = (drawable_size.x - total_width) / 2.0f;
    float start_y = (drawable_size.y - total_height) / 2.0f;


    for (int row = 0; row < num_rows; ++row) {
        for (int col = 0; col < num_cols; ++col) {
            Card const &card = board[row][col];
            
            //std::cout << "card (" << row << ", " << col << ") face up"  << card.face_up << "\n";

            glm::vec2 position(
                start_x + col * (card_size.x + card_spacing_x) + card_size.x / 2.0f,
                start_y + row * (card_size.y + card_spacing_y) + card_size.y / 2.0f
            );
            
            //std::cout << "Card (" << row << ", " << col << ") position: (" << position.x << ", " << position.y << ")\n";

            glm::u8vec4 color;
            if (!card.face_up) {
                color = glm::u8vec4(255, 255, 255, 255); //white
            } else {
                // Assign colors based on card value
                switch (card.value) {
                    case 0: color = glm::u8vec4(255, 0, 0, 255);   // Red
                        break;
                    case 1: color = glm::u8vec4(0, 255, 0, 255);   // Green
                        break;
                    case 2: color = glm::u8vec4(0, 0, 255, 255);   // Blue
                        break;
                    case 3: color = glm::u8vec4(255, 255, 0, 255); // Yellow
                        break;
                    case 4: color = glm::u8vec4(255, 165, 0, 255); // Orange
                        break;
                    case 5: color = glm::u8vec4(128, 0, 128, 255); // Purple
                        break;
                    default: color = glm::u8vec4(128, 128, 128, 255); // Gray
                        break;
                }
            }
            
            //std::cout << "card x: "<< position.x << " y : " << position.y << "\n";
            draw_card(position, card_size, color, vertices);
        }
        
       
    }
    
    //glm::u8vec4 color = glm::u8vec4(255, 255, 255, 255);
    int xMouse, yMouse;
    SDL_GetMouseState(&xMouse, &yMouse);
    //std::cout << "mouse click" << "x: " <<  xMouse << " y: " << yMouse << "\n";
    glm::vec4 mouse_pos =  glm::vec4(xMouse, yMouse, 0, 1);
    mouse_pos = projection * mouse_pos;
    mouse_pos /= mouse_pos.w;
    
    //std::cout << "mouse x : " << mouse_pos.x * drawable_size.x +  drawable_size.x << "y : " << - mouse_pos.y * drawable_size.y << "\n";
    
    //draw_card( glm::vec2(mouse_pos.x * drawable_size.x +  drawable_size.x,- mouse_pos.y * drawable_size.y), card_size, color, vertices);
    
     
    
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STREAM_DRAW);

    glBindVertexArray(vertex_buffer_for_color_texture_program);

    glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
    
    //draw player info
    {
        DrawLines lines(projection);
       
        auto draw_text = [&lines, &drawable_size](glm::vec2 const &text_pos, std::string const &text, float H) {
            lines.draw_text(text,
                            glm::vec3(text_pos.x, text_pos.y, 0.0),
                            glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                            glm::u8vec4(0x00, 0x00, 0x00, 0x00)); // Shadow
            float ofs = (1.0f / H) / drawable_size.y; // Offset for shadow
            lines.draw_text(text,
                            glm::vec3(text_pos.x + ofs, text_pos.y + ofs, 0.0),
                            glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                            glm::u8vec4(0xff, 0xff, 0xff, 0x00)); // Main text
        };

        for (size_t i = 0; i < players.size(); ++i) {
            glm::vec2 text_position(100.0f, drawable_size.y - (i + 1) * 100.0f);
            std::string text = "Player " + std::to_string(i + 1);
            draw_text(text_position, text, 0.09f);
            //std::cout << "Player " << i << ": " << text_position.x << ", " <<text_position.y << "\n";
        }
        
        //draw_text( glm::vec2(mouse_pos.x * drawable_size.x +  drawable_size.x,- mouse_pos.y * drawable_size.y), "hello", 0.09f);
    }
    

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    GL_ERRORS();
}

void PlayMode::draw_card(glm::vec2 const &center, glm::vec2 const &size, glm::u8vec4 const &color, std::vector<Vertex> &vertices) {
    glm::vec2 half_size = size * 0.5f;
    glm::vec2 default_tex_coord(0.0f, 0.0f);

    // Draw rectangle as two triangles
    vertices.emplace_back(glm::vec3(center.x - half_size.x, center.y - half_size.y, 0.0f), color, default_tex_coord);
    vertices.emplace_back(glm::vec3(center.x + half_size.x, center.y - half_size.y, 0.0f), color, default_tex_coord);
    vertices.emplace_back(glm::vec3(center.x + half_size.x, center.y + half_size.y, 0.0f), color, default_tex_coord);

    vertices.emplace_back(glm::vec3(center.x - half_size.x, center.y - half_size.y, 0.0f), color, default_tex_coord);
    vertices.emplace_back(glm::vec3(center.x + half_size.x, center.y + half_size.y, 0.0f), color, default_tex_coord);
    vertices.emplace_back(glm::vec3(center.x - half_size.x, center.y + half_size.y, 0.0f), color, default_tex_coord);
}
