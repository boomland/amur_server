#include <iostream>
#include <string>

using std::cout;

class Banan{
 public:
 	Banan(int y, std::string n): years(y), name(n) {}
 	void printAge() {
 		std::cout << "I have " << years << " years\n";
 	}
 	void printName() {
 		std::cout << "I am " << name << "!!\n"; 
 	}
 private:
 	int years;
 	std::string name;
};

int main() {
	Banan* me = new Banan(10, "Yaroslav");
	me->printAge();
	me->printName();
	printf("%d\n", me->years);
}


