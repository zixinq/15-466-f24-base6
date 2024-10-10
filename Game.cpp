#include "Game.hpp"

#include "Connection.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#include <glm/gtx/norm.hpp>


void Game::initialize_board() {
    std::vector<int> values;
    for (int i = 0; i < (rows * cols) / 2; ++i) {
        values.push_back(i);
        values.push_back(i);
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(values.begin(), values.end(), g);

    board.resize(rows, std::vector<Card>(cols));
    int index = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            board[r][c] = {values[index++], false, false};
        }
    }
}

void Game::update_player_score(int playerIndex, int points) {
    for (Player& player : players) {
        if (player.index == playerIndex) {
            player.score += points;
            break;
        }
    }
}
bool Game::flip_card(int cardIndex, Player &player) {
    int row = cardIndex / cols;
    int col = cardIndex % cols;
    Card &card = board[row][col];
    if (card.matched || card.face_up) {
        return false;
    }
    card.face_up = true; // Flip the card

    if (last_flipped_index < 0) {
        last_flipped_index = cardIndex;
    } else {
        Card &last_card = board[last_flipped_index / cols][last_flipped_index % cols];
        if (last_card.value == card.value) {
            card.matched = true;
            last_card.matched = true;
            update_player_score(current_player_index, 1);
        } else {
            schedule_flip_back(last_flipped_index, cardIndex);
        }
        last_flipped_index = -1;
    }
    return true;
}


void Game::reset_board() {
    for (auto &row : board) {
        for (auto &card : row) {
            card.face_up = false;
            card.matched = false;
        }
    }
}


//-----------------------------------------

Game::Game() : mt(0x15466666) {
    initialize_board();
}
/*
Player *Game::spawn_player() {
	players.emplace_back();
	Player &player = players.back();

	//random point in the middle area of the arena:
	player.position.x = glm::mix(ArenaMin.x + 2.0f * PlayerRadius, ArenaMax.x - 2.0f * PlayerRadius, 0.4f + 0.2f * mt() / float(mt.max()));
	player.position.y = glm::mix(ArenaMin.y + 2.0f * PlayerRadius, ArenaMax.y - 2.0f * PlayerRadius, 0.4f + 0.2f * mt() / float(mt.max()));

	do {
		player.color.r = mt() / float(mt.max());
		player.color.g = mt() / float(mt.max());
		player.color.b = mt() / float(mt.max());
	} while (player.color == glm::vec3(0.0f));
	player.color = glm::normalize(player.color);

	player.name = "Player " + std::to_string(next_player_number++);

	return &player;
}
*/

void Game::remove_player(Player *player) {
	bool found = false;
	for (auto pi = players.begin(); pi != players.end(); ++pi) {
		if (&*pi == player) {
			players.erase(pi);
			found = true;
			break;
		}
	}
	assert(found);
}


void Game::update(float elapsed) {
    if (waiting_for_flip_back && flip_back_timer > 0.0f) {
        flip_back_timer -= elapsed;
        if (flip_back_timer <= 0.0f) {
            // Flip back cards
            for (auto &row : board) {
                for (auto &card : row) {
                    if (!card.matched && card.face_up) {
                        card.face_up = false; // Flip card back down
                    }
                }
            }
            waiting_for_flip_back = false; // Reset the flip back flag
        }
    }

    for (Player &player : players) {
        if (player.has_selected_card) {
            // Attempt to flip the selected card
            bool success = flip_card(player.selected_card_index, player);
            
            if (success) {
                // Check if the game should end
                if (check_all_cards_matched()) {
                    end_game();
                }
            }
            player.has_selected_card = false; // Reset selection state
        }
    }
    
    // Check if all matches are found
    if (check_all_cards_matched()) {
        end_game();
    }
}


void Game::schedule_flip_back(int firstCardIndex, int secondCardIndex) {
    // Logic for scheduling cards to flip back
    waiting_for_flip_back = true;
    flip_back_timer = 1.0f; // Time after which cards should flip back
}

bool Game::check_all_cards_matched() {
    for (const auto &row : board) {
        for (const auto &card : row) {
            if (!card.matched) return false; // Found an unmatched card
        }
    }
    return true;
}

void Game::end_game() {
    std::cout << "Game over! Scores:\n";
    for (const auto &player : players) {
        std::cout << "Player " << player.index << ": " << player.score << "\n";
    }
}


void Game::send_state_message(Connection *connection_, Player *connection_player) const {
	assert(connection_);
	auto &connection = *connection_;

	connection.send(Message::S2C_State);
	//will patch message size in later, for now placeholder bytes:
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	size_t mark = connection.send_buffer.size(); //keep track of this position in the buffer


	auto send_player = [&](Player const &player) {
		connection.send(player.index);
		connection.send(player.score);
	};

	//player count:
	connection.send(uint8_t(players.size()));
	if (connection_player) send_player(*connection_player);
	for (auto const &player : players) {
		//if (&player == connection_player) continue;
		send_player(player);
	}
    
    // Send the board
    for (const auto &row : board) {
        for (const auto &card : row) {
            uint8_t card_info = (card.face_up ? 0x80 : 0x00) | (card.matched ? 0x40 : 0x00) | (card.value & 0x3F);
            connection.send(card_info);
        }
    }

	//compute the message size and patch into the message header:
	uint32_t size = uint32_t(connection.send_buffer.size() - mark);
	connection.send_buffer[mark-3] = uint8_t(size);
	connection.send_buffer[mark-2] = uint8_t(size >> 8);
	connection.send_buffer[mark-1] = uint8_t(size >> 16);
}

bool Game::recv_state_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;

	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::S2C_State)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	uint32_t at = 0;
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	//copy bytes from buffer and advance position:
	auto read = [&](auto *val) {
		if (at + sizeof(*val) > size) {
			throw std::runtime_error("Ran out of bytes reading state message.");
		}
		std::memcpy(val, &recv_buffer[4 + at], sizeof(*val));
		at += sizeof(*val);
	};

	players.clear();
	uint8_t player_count;
	read(&player_count);
	for (uint8_t i = 0; i < player_count; ++i) {
		players.emplace_back();
		Player &player = players.back();
		read(&player.index);
		read(&player.score);
	}
    
    for (auto &row : board) {
        for (auto &card : row) {
            uint8_t card_info;
            memcpy(&card_info, &recv_buffer[at], sizeof(card_info));
            at += sizeof(card_info);

            card.face_up = card_info & 0x80;
            card.matched = card_info & 0x40;
            card.value = card_info & 0x3F;
        }
    }

	if (at != 4 + size) throw std::runtime_error("Trailing data in state message.");

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}
