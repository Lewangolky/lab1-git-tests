#include <cassert>
#include <iostream>
#include "../main.cpp"

int main() {
    Inventory<Item> inv(3);

    // Тест 1: Додавання предмета
    assert(inv.add(Item("Sword", 10)) == true);
    assert(inv.snapshot().size() == 1);
    std::cout << "Тест 1 пройдено — додавання працює\n";

    // Тест 2: Перевірка переповнення інвентарю
    inv.add(Item("Shield", 20));
    inv.add(Item("Potion", 30));
    assert(inv.add(Item("Extra", 40)) == false);
    std::cout << "Тест 2 пройдено — перевірка переповнення працює\n";

    // Тест 3: Видалення предмета
    assert(inv.removeIfName("Shield") == true);
    assert(inv.findByName("Shield") == nullptr);
    std::cout << "Тест 3 пройдено — видалення працює\n";

    std::cout << "\nУсі юніт-тести для Inventory пройдено успішно!\n";
    return 0;
}
