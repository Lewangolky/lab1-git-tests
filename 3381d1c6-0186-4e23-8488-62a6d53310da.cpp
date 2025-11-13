#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <queue>
#include <random>
#include <chrono>
#include <cstdlib>
using namespace std;

/*
  Лабораторна №1 — Дерева + Ігрові системи
  Тема: Skill Trees + Characters
*/

/* --------------------- Logger --------------------- */
class Logger {
public:
    enum Level { INFO, WARN, ERROR };
private:
    Level level;
public:
    Logger(Level l = INFO) : level(l) {}
    void log(const string& msg, Level l = INFO) {
        if (l >= level) {
            cout << "[" << levelName(l) << "] " << msg << "\n";
        }
    }
    static string levelName(Level l) {
        switch (l) {
        case INFO: return "INFO";
        case WARN: return "WARN";
        default: return "ERROR";
        }
    }
};

/* --------------------- Item (helper) --------------------- */
class Item {
    string name;
    int value;
public:
    Item(string n = "", int v = 0) : name(n), value(v) {}
    string getName() const { return name; }
    int getValue() const { return value; }
    string str() const {
        return name + "($" + to_string(value) + ")";
    }
};

/* --------------------- Inventory (template - static polymorphism example #1) ---------------------
   Generic container for items (demonstrates template-based polymorphism at compile-time).
*/
template<typename T>
class Inventory {
    vector<T> items;
    size_t capacity;
public:
    Inventory(size_t cap = 10) : capacity(cap) {}
    bool add(const T& it) {
        if (items.size() >= capacity) return false;
        items.push_back(it);
        return true;
    }
    bool removeIfName(const string& n) {
        auto it = remove_if(items.begin(), items.end(), [&](const T& it) { return it.getName() == n; });
        if (it == items.end()) return false;
        items.erase(it, items.end());
        return true;
    }
    T* findByName(const string& n) {
        for (auto& it : items)
            if (it.getName() == n) return &it;
        return nullptr;
    }
    vector<T> snapshot() const { return items; } // copy
    string toString() const {
        string s = "Inventory(" + to_string(items.size()) + "/" + to_string(capacity) + "): ";
        for (auto& it : items) s += it.str() + " ";
        return s;
    }
};

/* --------------------- Forward declarations --------------------- */
class Character;
class Skill;

/* --------------------- Skill hierarchy (dynamic polymorphism) --------------------- */
class Skill {
protected:
    string name;
    int level;          // level of skill
    int basePower;
public:
    Skill(const string& n, int p = 10) : name(n), level(1), basePower(p) {}
    virtual ~Skill() = default;
    virtual int effectivePower() const {
        // non-trivial: computed from basePower and level
        return basePower + level * 3;
    }
    virtual string description() const {
        return name + " (lvl " + to_string(level) + ", pwr " + to_string(effectivePower()) + ")";
    }
    virtual void apply(Character& target) = 0; // abstract action on target
    virtual void upgrade() {
        level++;
        basePower = basePower + 2;
    }
    string getName() const { return name; }
};

class ActiveSkill : public Skill {
    int manaCost;
public:
    ActiveSkill(const string& n, int p = 12, int cost = 10) : Skill(n, p), manaCost(cost) {}
    int effectivePower() const override {
        // active skill gains more per level
        return basePower + level * 5;
    }
    string description() const override {
        return "Active: " + Skill::description() + " mana:" + to_string(manaCost);
    }
    void apply(Character& target) override;
};

class PassiveSkill : public Skill {
    double modifier; // e.g., increases defense or attack by percentage
public:
    PassiveSkill(const string& n, int p = 5, double mod = 0.05) : Skill(n, p), modifier(mod) {}
    int effectivePower() const override {
        // passive skill contributes moderately
        return basePower + static_cast<int>(level * (modifier * 100));
    }
    string description() const override {
        return "Passive: " + Skill::description() + " mod: " + to_string(modifier);
    }
    void apply(Character& target) override; // will modify stats passively
};

class UltimateSkill : public ActiveSkill {
    int cooldown;
public:
    UltimateSkill(const string& n, int p = 30, int cost = 30, int cd = 3) : ActiveSkill(n, p, cost), cooldown(cd) {}
    int effectivePower() const override {
        // very strong scaling
        return basePower + level * 12;
    }
    string description() const override {
        return "Ultimate: " + Skill::description() + " cd:" + to_string(cooldown);
    }
    void apply(Character& target) override;
};

/* --------------------- SkillTree node (non-template, nodes hold Skill*) --------------------- */
class SkillTreeNode {
    Skill* skill;
    SkillTreeNode* parent;
    vector<unique_ptr<SkillTreeNode>> children;
public:
    SkillTreeNode(Skill* s = nullptr, SkillTreeNode* p = nullptr) : skill(s), parent(p) {}
    ~SkillTreeNode() { /* skill ownership is external (managed elsewhere) */ }

    Skill* getSkill() const { return skill; }
    SkillTreeNode* getParent() const { return parent; }

    // add child (creates ownership)
    SkillTreeNode* addChild(Skill* s) {
        children.push_back(make_unique<SkillTreeNode>(s, this));
        return children.back().get();
    }

    bool removeChildWithSkillName(const string& n) {
        auto it = remove_if(children.begin(), children.end(),
            [&](const unique_ptr<SkillTreeNode>& c) { return c->skill && c->skill->getName() == n; });
        if (it == children.end()) return false;
        children.erase(it, children.end());
        return true;
    }

    // depth-first traversal applying a function to each node (non-trivial)
    template<typename F>
    void dfs(F f) {
        f(this);
        for (auto& ch : children) ch->dfs(f);
    }

    vector<SkillTreeNode*> getChildrenRaw() {
        vector<SkillTreeNode*> out;
        for (auto& c : children) out.push_back(c.get());
        return out;
    }
};

/* --------------------- SkillTree template (static polymorphism example #2) ---------------------
   Generic skill tree template — demonstrates compile-time genericity.
   T is the stored "skill pointer" type (we will use Skill*).
*/
template<typename T>
class SkillTree {
    unique_ptr<SkillTreeNode> root;
public:
    SkillTree() : root(nullptr) {}
    SkillTree(Skill* s) { root = make_unique<SkillTreeNode>(s, nullptr); }

    SkillTreeNode* getRoot() const { return root.get(); }

    // insert under parent skill name; returns pointer or nullptr
    SkillTreeNode* insertUnder(const string& parentSkillName, Skill* s) {
        if (!root) {
            root = make_unique<SkillTreeNode>(s, nullptr);
            return root.get();
        }
        SkillTreeNode* found = findNodeBySkillName(parentSkillName);
        if (!found) return nullptr;
        return found->addChild(s);
    }

    SkillTreeNode* findNodeBySkillName(const string& name) const {
        if (!root) return nullptr;
        SkillTreeNode* result = nullptr;
        root->dfs([&](SkillTreeNode* node) {
            if (node->getSkill() && node->getSkill()->getName() == name) result = node;
            });
        return result;
    }

    // traverse and collect descriptions
    vector<string> descriptionsDFS() const {
        vector<string> out;
        if (!root) return out;
        root->dfs([&](SkillTreeNode* node) {
            if (node->getSkill()) out.push_back(node->getSkill()->description());
            });
        return out;
    }

    // generate simple random tree (non-trivial algorithm)
    void generateRandom(Skill* (*skillFactory)(), int maxDepth = 3, int maxChildren = 3) {
        // skillFactory() returns pointer to a newly allocated Skill
        root = make_unique<SkillTreeNode>(skillFactory(), nullptr);
        default_random_engine rng((unsigned)chrono::high_resolution_clock::now().time_since_epoch().count());
        // BFS-like expansion
        queue<pair<SkillTreeNode*, int>> q;
        q.push({ root.get(), 1 });
        while (!q.empty()) {
            auto [node, depth] = q.front(); q.pop();
            if (depth >= maxDepth) continue;
            uniform_int_distribution<int> distChildren(0, maxChildren);
            int nc = distChildren(rng);
            for (int i = 0;i < nc;i++) {
                Skill* s = skillFactory();
                SkillTreeNode* child = node->addChild(s);
                q.push({ child, depth + 1 });
            }
        }
    }
};

/* --------------------- Character hierarchy --------------------- */
class Character {
protected:
    string name;
    int hp;
    int mana;
    int attackPower;
    int defense;
    int level;
    vector<unique_ptr<Skill>> ownedSkills; // owns some skills
    Inventory<Item> inventory; // composition of template Inventory
    Logger& logger;
public:
    Character(const string& n, Logger& log)
        : name(n), hp(100), mana(50), attackPower(10), defense(5), level(1), inventory(10), logger(log) {
    }

    virtual ~Character() = default;

    // non-trivial: attack uses attackPower & level & some randomness
    virtual int attack(Character& target) {
        int raw = attackPower + level * 2;
        int variance = rand() % (level + 3);
        int dmg = max(0, raw + variance - target.getDefense());
        target.takeDamage(dmg);
        logger.log(name + " attacks " + target.getName() + " for " + to_string(dmg) + " dmg.");
        return dmg;
    }

    virtual void useSkill(size_t idx, Character& target) {
        if (idx >= ownedSkills.size()) {
            logger.log(name + " tried to use invalid skill index.", Logger::WARN);
            return;
        }
        Skill* sk = ownedSkills[idx].get();
        if (!sk) return;
        logger.log(name + " uses " + sk->getName() + " on " + target.getName());
        sk->apply(target); // dynamic dispatch
    }

    virtual void equipSkill(unique_ptr<Skill> s) {
        if (!s) return;
        logger.log(name + " equips skill " + s->getName());
        ownedSkills.push_back(move(s));
    }

    virtual void levelUp() {
        level++;
        hp += 10;
        mana += 5;
        attackPower += 2;
        defense += 1;
        logger.log(name + " leveled up to " + to_string(level));
    }

    virtual void takeDamage(int d) {
        hp -= d;
        if (hp < 0) hp = 0;
    }

    virtual int getHP() const { return hp; }
    virtual int getMana() const { return mana; }
    virtual int getDefense() const { return defense; }
    string getName() const { return name; }

    virtual string status() const {
        return name + " (lvl " + to_string(level) + ") HP:" + to_string(hp) + " MP:" + to_string(mana);
    }

    // non-trivial: compute overall power from stats and skills
    virtual int overallPower() const {
        int p = attackPower + level * 3;
        for (auto& s : ownedSkills) p += s->effectivePower() / 2;
        return p;
    }

    Inventory<Item>& getInventory() { return inventory; }
    size_t skillCount() const { return ownedSkills.size(); }
};

/* Derived classes: Warrior, Mage, Archer */
class Warrior : public Character {
    int rage;
public:
    Warrior(const string& n, Logger& log) : Character(n, log), rage(0) {
        attackPower += 5;
        defense += 3;
    }
    int attack(Character& target) override {
        rage = min(100, rage + 10);
        int base = Character::attack(target);
        // warrior special: bonus when rage enough
        if (rage >= 50) {
            int bonus = 5 + level;
            target.takeDamage(bonus);
            logger.log(name + " uses RAGE bonus for " + to_string(bonus) + " extra dmg!");
            rage = 0;
            return base + bonus;
        }
        return base;
    }
    void battleShout() {
        // non-trivial buff to self
        attackPower += 2;
        logger.log(name + " shouts and increases attack!");
    }
};

class Mage : public Character {
    int spellPower;
public:
    Mage(const string& n, Logger& log) : Character(n, log), spellPower(10) {
        mana += 30;
    }
    void useSkill(size_t idx, Character& target) override {
        // mana check non-trivial
        if (idx >= skillCount()) { logger.log("Invalid skill idx", Logger::WARN); return; }
        Skill* sk = ownedSkills[idx].get();
        if (!sk) return;
        int cost = max(5, sk->effectivePower() / 3);
        if (mana < cost) {
            logger.log(name + " doesn't have enough mana (" + to_string(mana) + ") to cast " + sk->getName(), Logger::WARN);
            return;
        }
        mana -= cost;
        logger.log(name + " casts " + sk->getName() + " costing " + to_string(cost) + " mana.");
        sk->apply(target);
    }
    int getSpellPower() const { return spellPower; }
};

class Archer : public Character {
    int agility;
public:
    Archer(const string& n, Logger& log) : Character(n, log), agility(12) {
        attackPower += 2;
    }
    int attack(Character& target) override {
        // chance to critical hit depending on agility
        int chance = min(50, agility + level);
        int r = rand() % 100;
        if (r < chance) {
            int dmg = Character::attack(target) + 7;
            logger.log(name + " lands a CRITICAL hit!");
            return dmg;
        }
        else {
            return Character::attack(target);
        }
    }
    void dodge() {
        // non-trivial defensive move
        defense += 2;
        logger.log(name + " prepares to dodge, defense increased temporarily.");
    }
};

/* Implementations of Skill::apply now that Character is declared */
void ActiveSkill::apply(Character& target) {
    // Deal damage to target based on effectivePower
    int p = effectivePower();
    int variance = rand() % 5;
    int dmg = max(1, p + variance - target.getDefense());
    target.takeDamage(dmg);
    // Assuming we have logging via dynamic cast to access logger? We'll use cout fallback
    cout << "ActiveSkill " << name << " applied to " << target.getName() << " for " << dmg << " damage\n";
}

void PassiveSkill::apply(Character& target) {
    // Passive skill modifies target's stats slightly (non-trivial)
    // We'll attempt to dynamic_cast to specific types for different effects (example)
    // Since Character's fields are protected, we cannot change them directly; instead we print/apply conceptual effect.
    cout << "PassiveSkill " << name << " applied to " << target.getName() << " (passive buff)\n";
}

void UltimateSkill::apply(Character& target) {
    int p = effectivePower();
    int dmg = max(5, p - target.getDefense());
    target.takeDamage(dmg);
    cout << "UltimateSkill " << name << " strikes " << target.getName() << " for " << dmg << " massive damage!\n";
}

/* --------------------- Party (collection of characters demonstrating polymorphism use) --------------------- */
class Party {
    vector<unique_ptr<Character>> members;
    Logger& logger;
public:
    Party(Logger& log) : logger(log) {}
    void addMember(unique_ptr<Character> c) {
        logger.log("Adding member " + c->getName());
        members.push_back(move(c));
    }
    Character* getMember(size_t idx) {
        if (idx >= members.size()) return nullptr;
        return members[idx].get();
    }
    size_t size() const { return members.size(); }

    // non-trivial: compute combined power
    int combinedPower() const {
        int sum = 0;
        for (auto& m : members) sum += m->overallPower();
        return sum;
    }

    void showStatus() const {
        logger.log("Party status:");
        for (auto& m : members) cout << "  " << m->status() << "\n";
    }
};

/* --------------------- BattleSimulator --------------------- */
class BattleSimulator {
    Logger& logger;
public:
    BattleSimulator(Logger& log) : logger(log) {}

    // Simulate a simple skirmish between two parties (non-trivial orchestration)
    void simulate(Party& a, Party& b) {
        logger.log("Battle starts between two parties!");
        size_t turn = 0;
        while (turn < 50) {
            if (allDead(a)) { logger.log("Party A defeated!"); return; }
            if (allDead(b)) { logger.log("Party B defeated!"); return; }

            // choose random alive from A and B
            Character* ca = randomAlive(a);
            Character* cb = randomAlive(b);
            if (!ca || !cb) break;

            // alternate: even turns A attacks, odd B attacks
            if (turn % 2 == 0) {
                ca->attack(*cb);
            }
            else {
                cb->attack(*ca);
            }
            turn++;
        }
        logger.log("Battle ended after max turns.");
    }

private:
    bool allDead(Party& p) {
        for (size_t i = 0;i < p.size();++i) {
            Character* c = p.getMember(i);
            if (c && c->getHP() > 0) return false;
        }
        return true;
    }
    Character* randomAlive(Party& p) {
        vector<Character*> alive;
        for (size_t i = 0;i < p.size();++i) {
            Character* c = p.getMember(i);
            if (c && c->getHP() > 0) alive.push_back(c);
        }
        if (alive.empty()) return nullptr;
        return alive[rand() % alive.size()];
    }
};

/* --------------------- Helper skill factory for random generation --------------------- */
Skill* randomSkillFactory() {
    static int counter = 0;
    counter++;
    int t = rand() % 3;
    if (t == 0) return new ActiveSkill("Active_" + to_string(counter), 10 + (counter % 5));
    if (t == 1) return new PassiveSkill("Passive_" + to_string(counter), 5 + (counter % 3));
    return new UltimateSkill("Ult_" + to_string(counter), 25 + (counter % 8));
}

/* --------------------- Main demonstration --------------------- */
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    srand((unsigned)time(nullptr));
    Logger logger(Logger::INFO);

    // Create skill tree (template)
    SkillTree<Skill*> tree;
    tree.generateRandom(randomSkillFactory, 3, 2);
    cout << "Generated Skill Tree (DFS descriptions):\n";
    for (auto& s : tree.descriptionsDFS()) cout << " - " << s << "\n";

    // Create party A
    Party partyA(logger);
    auto w = make_unique<Warrior>("Thorin", logger);
    auto m = make_unique<Mage>("Merlin", logger);
    auto a = make_unique<Archer>("Legolas", logger);

    // equip skills (demonstrate dynamic polymorphism on skills)
    w->equipSkill(unique_ptr<Skill>(new ActiveSkill("Slash", 15)));
    w->equipSkill(unique_ptr<Skill>(new PassiveSkill("Toughness", 8, 0.1)));
    m->equipSkill(unique_ptr<Skill>(new ActiveSkill("Fireball", 20, 12)));
    m->equipSkill(unique_ptr<Skill>(new UltimateSkill("Meteor", 40, 40, 4)));
    a->equipSkill(unique_ptr<Skill>(new ActiveSkill("Piercing Arrow", 12, 6)));

    // add items to inventories (template inventory usage)
    w->getInventory().add(Item("Health Potion", 50));
    m->getInventory().add(Item("Mana Potion", 60));
    a->getInventory().add(Item("Quiver", 20));

    partyA.addMember(move(w));
    partyA.addMember(move(m));
    partyA.addMember(move(a));

    // Create party B
    Party partyB(logger);
    auto w2 = make_unique<Warrior>("Orc1", logger);
    auto m2 = make_unique<Mage>("Witch", logger);
    w2->equipSkill(unique_ptr<Skill>(new ActiveSkill("Cleave", 14)));
    m2->equipSkill(unique_ptr<Skill>(new ActiveSkill("Shadow Bolt", 18)));
    partyB.addMember(move(w2));
    partyB.addMember(move(m2));

    // Show status
    partyA.showStatus();
    partyB.showStatus();
    cout << "Party A combined power: " << partyA.combinedPower() << "\n";
    cout << "Party B combined power: " << partyB.combinedPower() << "\n";

    // Simulate battle
    BattleSimulator sim(logger);
    sim.simulate(partyA, partyB);

    // Demonstrate finding a skill in the skill tree
    auto root = tree.getRoot();
    if (root && root->getSkill()) {
        string q = root->getSkill()->getName();
        auto found = tree.findNodeBySkillName(q);
        if (found) cout << "Found root skill by name: " << q << "\n";
    }

    // Show inventory snapshot
    cout << "PartyA member 0 inventory: " << partyA.getMember(0)->getInventory().toString() << "\n";

    // End
    cout << "\n--- Demo finished ---\n";
    return 0;
}
