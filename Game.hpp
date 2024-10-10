#pragma once

#include <glm/glm.hpp>

#include <string>
#include <list>
#include <random>

struct Connection;

//Game state, separate from rendering.

//Currently set up for a "client sends controls" / "server sends whole state" situation.

enum class Message : uint8_t {
	//C2S_Controls = 1, //Greg!
    C2S_SelectCard = 0,
	S2C_State = 's',
	//...
};

struct Card {
    int value;       // Card value for matching
    bool face_up;    // Is the card face-up?
    bool matched;    // Has the card been matched?
};


//state of one player in the game:
struct Player {
    int index; // Unique identifier for each player
    int score;
    bool has_selected_card = false;
    int selected_card_index = -1;
	//player state (sent from server):
    /*
	glm::vec2 position = glm::vec2(0.0f, 0.0f);
	glm::vec2 velocity = glm::vec2(0.0f, 0.0f);

	glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
	std::string name = "";
     */
};

struct Game {
	std::list< Player > players; //(using list so they can have stable addresses)
    std::vector<std::vector<Card>> board;  // The game board with rows and columns
    // Number of rows in the game board
    int rows = 4;
    int cols = 4;
    int current_player_index = 0;
    int last_flipped_index = -1;
    bool waiting_for_flip_back = false;
    float flip_back_timer = 0.0f;
    int first_card_index = -1;
    
	Player *spawn_player(); //add player the end of the players list (may also, e.g., play some spawn anim)
	void remove_player(Player *); //remove player from game (may also, e.g., play some despawn anim)
    
	std::mt19937 mt; //used for spawning players
	uint32_t next_player_number = 1; //used for naming players
    void schedule_flip_back(int firstCardIndex, int secondCardIndex);

	Game();
    void initialize_board();
    bool flip_card(int cardIndex, Player &player);
    void update_player_score(int playerIndex, int points);
    void reset_board();  // Reset the board for a new game
    void send_state_message(Connection *connection) const;  // Send game state to a client
    
	//state update function:
	void update(float elapsed);
    void update_scores();
    void end_game();
    bool check_all_cards_matched();
	//constants:
	//the update rate on the server:
	inline static constexpr float Tick = 1.0f / 30.0f;

    /*
	//arena size:
	inline static constexpr glm::vec2 ArenaMin = glm::vec2(-0.75f, -1.0f);
	inline static constexpr glm::vec2 ArenaMax = glm::vec2( 0.75f,  1.0f);
    

	//player constants:
	inline static constexpr float PlayerRadius = 0.06f;
	inline static constexpr float PlayerSpeed = 2.0f;
	inline static constexpr float PlayerAccelHalflife = 0.25f;
    */
    
    inline static constexpr float PlayerRadius = 0.06f;
	
	//---- communication helpers ----

	//used by client:
	//set game state from data in connection buffer
	// (return true if data was read)
	bool recv_state_message(Connection *connection);

	//used by server:
	//send game state.
	//  Will move "connection_player" to the front of the front of the sent list.
	void send_state_message(Connection *connection, Player *connection_player = nullptr) const;
};
