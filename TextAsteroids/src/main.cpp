#include <iostream>
#include <string>
#include <array>
#include <thread>
#include <mutex>
#include <vector>
#include <deque>
#include <random>
#include <cctype>

#include <conio.h>
#include <Windows.h>


struct point
{
    short x;
    short y;

	point() : x(0), y(0) {}
	point(short x, short y) : x(x), y(y) {}
};

auto stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

static const short width = 60;
static const short height = 34;

std::mutex iomutex;

void SetConsoleSize(point size)
{
    COORD coord{ size.x, size.y };
    SMALL_RECT rect{ 0, 0, size.x - 1, size.y - 1 };
    CONSOLE_FONT_INFOEX info{ sizeof(info), static_cast<DWORD>(-1),{ 8, 14 }, FF_DONTCARE, 400, L"Lucida Console" };


    auto res = SetConsoleWindowInfo(stdout_handle, true, &rect);
    
    res = res && SetConsoleScreenBufferSize(stdout_handle, coord);

    res = res && SetCurrentConsoleFontEx(stdout_handle, false, &info);

	if (!res) throw std::runtime_error("failed to set console attributes");
}

void DisableInputEcho()
{
	auto stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode = 0;
	GetConsoleMode(stdin_handle, &mode);
	SetConsoleMode(stdin_handle, mode & (~ENABLE_ECHO_INPUT));
}

void SetCursorPos(point pos)
{
	std::unique_lock<std::mutex> lock(iomutex);
    COORD coord{pos.x, pos.y};
    SetConsoleCursorPosition(stdout_handle, { coord.X, coord.Y });
}

void WriteToConsoleBuffer(const char* chars, std::size_t length, point coords)
{
	std::unique_lock<std::mutex> lock(iomutex);
	DWORD written = 0;
	WriteConsoleOutputCharacterA(stdout_handle, chars, length, { coords.x, coords.y }, &written);
}


const float pi = 3.1415926535897932f;
const point window_size  = { width, height - 2 };
const point game_size    = { width, height - 3 };
const point input_coords = { 0 , 32 };

bool close_enough(float a, float b, float epsilon = std::numeric_limits<float>::epsilon())
{
	return std::abs(a - b) < epsilon;
}

struct RocketShip
{
	point position = { game_size.x / 2, game_size.y / 2 };
	point velocity = { 0,0 };
	float rotation = -pi/2;
	float angular_velocity = 0;

};

template<typename T>
T spaceship_cast(RocketShip& rocket)
{
	if (close_enough(rocket.rotation, -pi / 2, pi/4))
	{
		return 'A';
	}

	if (close_enough(rocket.rotation, pi / 2, pi / 4))
	{
		return 'V';
	}

	if (close_enough(rocket.rotation, pi, pi / 4) || close_enough(rocket.rotation, -pi, pi / 4))
	{
		return '<';
	}

	if (close_enough(rocket.rotation, 0, pi / 4))
	{
		return '>';
	}

	return 'X';
}

void step_simulation(RocketShip& ship)
{
	ship.position.x += ship.velocity.x;
	ship.position.y += ship.velocity.y;

	if (ship.position.x < 0)
		ship.position.x += game_size.x;

	if (ship.position.x >= game_size.x)
		ship.position.x -= game_size.x;

	if (ship.position.y < 0)
		ship.position.y += game_size.y;

	if (ship.position.y >= game_size.y)
		ship.position.y -= game_size.y;

	ship.rotation += ship.angular_velocity;

	if (ship.rotation > pi)
		ship.rotation -= 2*pi;

	if (ship.rotation < -pi)
		ship.rotation += 2 * pi;
}

struct Asteroid
{
	point position;
	point velocity;
	int health;

	float radius;

	Asteroid() {}
};

float distance(point A, point B)
{
	float dx = B.x - A.x;
	float dy = B.y - A.y;
	return std::sqrt(dx*dx + dy*dy);
}

void draw_asteroid(Asteroid& roid)
{
	for (short y = roid.position.y - roid.radius; y < roid.position.y + roid.radius; y++)
	{
		for (short x = roid.position.x - roid.radius; x < roid.position.x + roid.radius; x++)
		{
			if (distance({ x,y }, roid.position) < roid.radius)
			{
				if (x >= 0 && x < game_size.x && y >= 0 && y < game_size.y)
				{
					char c = '#';

					WriteToConsoleBuffer(&c, 1, { x, y });
				}

			}
		}
	}
}

void step_simulation(Asteroid& roid)
{
	roid.position.x += roid.velocity.x;
	roid.position.y += roid.velocity.y;
}

struct Projectile
{
	point position;
	point velocity;

	Projectile() {};
};

void step_simulation(Projectile& projectile)
{
	projectile.position.x += projectile.velocity.x;
	projectile.position.y += projectile.velocity.y;
}

bool line_circle_intersect(point A, point B, point center, float radius)
{
	float d = distance(A, B);
	float det = A.x * B.y - B.x * A.y;
	float discr = radius*radius * (d*d) - (det * det);

	return discr >= 0;
}

bool circle_circle_intersection(point center1, float radius1, point center2, float radius2)
{
	float distx = center2.x - center1.x;
	float disty = center2.y - center1.y;
	float dist = std::sqrt(distx*distx + disty*disty);

	return dist < (radius1 + radius2);
}

int main()
{

	const std::size_t tick_interval_ms = 500;

	std::vector<char> screen_buffer(game_size.x * game_size.y);

	SetConsoleSize({ width,height });
	DisableInputEcho();

	std::mutex simulation_mutex;

	std::string buf;
	std::size_t offset = 0;
	std::string input;

	const std::string blank(width, ' ');
	bool game_over = false;

	RocketShip ship;

	std::vector<Asteroid> roids;
	std::vector<Projectile> projectiles;


	auto Render = [&]
	{

		//clear
		for (size_t i = 0; i < game_size.y; i++)
		{
			WriteToConsoleBuffer(blank.data(), width, { 0, static_cast<short>(i) });
		}
		
		for (auto& roid : roids)
		{
			draw_asteroid(roid);
		}

		for (auto& proj : projectiles)
		{
			char c = '*';
			WriteToConsoleBuffer(&c, 1, proj.position);
		}

		if (!game_over)
		{
			auto ship_char = spaceship_cast<char>(ship);
			WriteToConsoleBuffer(&ship_char, 1, ship.position);
		}

		if (game_over)
		{
			const std::string game_over_screen =
				"=========================GAME OVER========================="
				"            You got blown up by an asteroid!               "
				"                                                           "
				"                   press [enter] to exit                   ";

			WriteToConsoleBuffer(game_over_screen.data(), game_over_screen.length(), { 0, 6 });

			return;
		}
	};

	
    auto PollInput = [&]
    {
		auto ch = _getch();
		
		switch (ch)
		{
			case 13: //carriage return
			{
				if (game_over)
					std::exit(0);

				input = buf;
				buf.clear();
				offset = 0;
				break;
			}
			case 8: //backspace
			{
				if (buf.length())
				{
					buf.erase(buf.begin() + offset - 1);
					offset--;
				}

				break;
			}
			case 224: //some keys
			{
				auto ch2 = _getch();

				switch (ch2)
				{
					case 83: //DEL
					{
						buf.erase(buf.begin() + offset);
						break;
					}
					case 71: //HOME
					{
						offset = 0;
						break;
					}
					case 79: //END
					{
						offset = buf.length();
						break;
					}
					case 75: //LEFT
					{
						offset = min(buf.length(), max(0, offset - 1));
						break;
					}
					case 77: //RIGHT
					{
						offset = min(buf.length(), max(0, offset + 1));
						break;
					}
				}
				break;
			}
			case 0: //other keys
			{
				_getch();
				break;
			}
			default:
			{
				if (std::isprint(ch))
					if (buf.length() < width - 1)
					{
						buf.insert(buf.begin() + offset, ch);
						offset++;
					}
				break;
			}
		}
        
    };

	auto ActOnInput = [&]
	{
		if (!input.empty())
		{
			std::unique_lock<std::mutex> lock(simulation_mutex);

			if (input == "pew")
			{
				Projectile p;
				p.position = ship.position;
				p.velocity = { static_cast<short>(2 * cos(ship.rotation)), static_cast<short>(2 * sin(ship.rotation)) };

				projectiles.push_back(p);
			}
			if (input == "w")
			{
				ship.velocity.y -= 1;
			}
			if (input == "a")
			{
				ship.velocity.x -= 1;
			}
			if (input == "s")
			{
				ship.velocity.y += 1;
			}
			if (input == "d")
			{
				ship.velocity.x += 1;
			}
			if (input == "e")
			{
				ship.angular_velocity += pi / 8;
			}
			if (input == "q")
			{
				ship.angular_velocity -= pi / 8;
			}
			input.clear();
		}
	};

	std::size_t ticks_per_roid = 5;

	auto SimulateRoids = [&]
	{
		static std::mt19937 twister(0);
		static std::uniform_int_distribution<int> xdist(0, game_size.x);


		for (auto& roid : roids)
		{
			step_simulation(roid);
		}

		//delete roids that have left the area
		std::remove_if(roids.begin(), roids.end(), [](const Asteroid& roid)
		{
			return roid.position.x < 0 || roid.position.x >= game_size.x
				|| roid.position.y < 0 || roid.position.y >= game_size.y;
		});



		//spawn new roids
		static std::size_t ticks = 0;
		if (ticks == 0)
		{
			Asteroid roid;
			roid.radius = 2;
			roid.position.x = xdist(twister);
			roid.position.y = 0;
			roid.velocity.x = std::uniform_int_distribution<int>(-2, 2)(twister);
			roid.velocity.y = 1;
			roid.health = std::uniform_int_distribution<int>(10, 50)(twister);
			roids.push_back(roid);
		}

		ticks = (ticks + 1) % ticks_per_roid;

	};

	auto SimulateShip = [&]
	{
		step_simulation(ship);

		for (auto& roid : roids)
		{
			if (circle_circle_intersection(ship.position, std::sqrt(2), roid.position, roid.radius))
			{
				game_over = true;
			}
		}
	};

	auto SimulateProjectiles = [&]
	{
		for (auto& proj : projectiles)
		{
			step_simulation(proj);
		}

		//delete projectiles that have left the area
		std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& proj)
		{
			return proj.position.x < 0 || proj.position.x >= game_size.x
				|| proj.position.y < 0 || proj.position.y >= game_size.y;
		});

		for (auto& proj : projectiles)
		{
			for (auto& roid : roids)
			{
				if (circle_circle_intersection(proj.position, std::sqrt(2), roid.position, roid.radius))
				{
					//mark roid and projectile to be deleted next tick
					proj.position.x = -1;
					roid.position.x = -1;
				}
			}
		}


	};

	std::thread get_input([&]
	{
		while (true)
		{
			SetCursorPos({ static_cast<short>(input_coords.x + offset), input_coords.y });

			PollInput();

			DWORD written = 0;

			WriteToConsoleBuffer(blank.data(), blank.length(), input_coords);
			WriteToConsoleBuffer(buf.data(), buf.length(), input_coords);

			ActOnInput();
		}
	});

	while (true)
	{
		{
			std::unique_lock<std::mutex> lock(simulation_mutex);

			SimulateProjectiles();

			SimulateRoids();

			SimulateShip();


			Render();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(tick_interval_ms));
	}

	get_input.join();

    return 0;
}
