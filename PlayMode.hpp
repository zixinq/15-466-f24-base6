#include "Mode.hpp"

#include "Connection.hpp"
#include "Game.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <string>

#include "GL.hpp"

struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &drawable_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;
    
    void flip_card(int row, int col);    // Flip a card at (row, col)
    void next_turn();                    // Switch to the next player
    bool get_card_from_mouse(glm::vec2 const &mouse_pos, int &row, int &col, glm::uvec2 const &window_size);  // Convert mouse position to card row/col
    
	//----- game state -----

	//input tracking for local player:
	//Player::Controls controls;

	//latest game state (from server):
	//Game game;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

    //playing cards
    struct Card {
        int value;
        bool face_up;
        bool matched;
    };
    
    struct Player {
        int index;
        int score = 0;
        bool has_selected_card = false;
        int selected_card_index = -1;

        // Send and receive player state
        void send_state(Connection* connection) const;

        void recv_state(Connection* connection);
    };
    
    Player local_player;           
    std::vector<Player> players;
    
    std::vector<std::vector<Card>> board;
    GLuint front_tex;
    GLuint back_tex;
    
    int rows = 4;
    int cols = 4;  // Board dimensions (4x4 grid)
    bool game_over = false;
    int current_player = 0;  // Track the current player's turn

    // Pointers to the two cards being flipped
    Card* first_flip = nullptr;
    Card* second_flip = nullptr;

    struct Vertex {
            Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_, glm::vec2 const &TexCoord_) :
                Position(Position_), Color(Color_), TexCoord(TexCoord_) { }
            glm::vec3 Position;
            glm::u8vec4 Color;
            glm::vec2 TexCoord;
        };
    
    void draw_card(glm::vec2 const &center, glm::vec2 const &size, glm::u8vec4 const &color, std::vector<Vertex> &vertices);
    GLuint vertex_buffer = 0; // buffer for tristrip
    GLuint vertex_buffer_for_color_texture_program = 0; // vao
    
    
    
    /*
    struct {
        GLuint tex = 0;
        GLuint vertex_buffer = 0; // buffer for tristrip
        GLuint vertex_buffer_for_texture_program = 0; // vao
        GLsizei count = 0;
        glm::mat4 OBJECT_TO_CLIP = glm::mat4(1.f);
    } front_card, back_card;
     */


};
