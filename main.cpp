// ============================================================
//  Bank Management System  -  C++11 or newer  -  Object Oriented
//  Compile:  g++ -std=c++11 -o bank BankManagementSystem.cpp
//  Run:      ./bank      (Windows: bank.exe)
//  Data is saved automatically in "bank_data.txt"
// ============================================================
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
 
using namespace std;
 
// ------------------------- Helpers --------------------------
string currentTime() {
    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&now));
    return string(buf);
}
 
string sanitize(string s) {  // '|' is used as a separator in the data file
    replace(s.begin(), s.end(), '|', ' ');
    return s;
}
 
vector<string> split(const string& s, char delim) {
    vector<string> parts;
    string item;
    stringstream ss(s);
    while (getline(ss, item, delim)) parts.push_back(item);
    return parts;
}
 
string readLine(const string& prompt) {
    string line;
    cout << prompt;
    getline(cin, line);
    if (!cin) {  // input closed (Ctrl+D / Ctrl+Z)
        cout << "\nInput closed. Exiting...\n";
        exit(0);
    }
    return line;
}
 
string readNonEmpty(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        size_t start = s.find_first_not_of(" \t");
        if (start != string::npos) {
            size_t end = s.find_last_not_of(" \t");
            return s.substr(start, end - start + 1);
        }
        cout << "  Input cannot be empty.\n";
    }
}
 
int readInt(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        try {
            size_t pos;
            int v = stoi(s, &pos);
            if (pos == s.size()) return v;
        } catch (...) {
        }
        cout << "  Please enter a valid whole number.\n";
    }
}
 
double readAmount(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        try {
            size_t pos;
            double v = stod(s, &pos);
            if (pos == s.size() && v > 0 && isfinite(v)) return v;
        } catch (...) {
        }
        cout << "  Please enter a valid positive amount.\n";
    }
}
 
string readPin(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        bool ok = s.size() == 4 &&
                  all_of(s.begin(), s.end(), [](unsigned char c) { return isdigit(c); });
        if (ok) return s;
        cout << "  PIN must be exactly 4 digits.\n";
    }
}
 
// ------------------------- Transaction ----------------------
struct Transaction {
    string date;
    string type;
    double amount;
    double balanceAfter;
    string note;
};
 
// ------------------------- Account (abstract) ---------------
class Account {
protected:
    int id;
    string holderName;
    string pin;
    double balance;
    vector<Transaction> history;
 
    void record(const string& type, double amount, const string& note) {
        history.push_back({currentTime(), type, amount, balance, note});
    }
 
public:
    Account(int id, string name, string pin, double balance)
        : id(id), holderName(move(name)), pin(move(pin)), balance(balance) {}
    virtual ~Account() = default;
 
    // ---- polymorphic interface ----
    virtual string typeName() const = 0;
    virtual double availableToWithdraw() const = 0;
    virtual void display() const {
        cout << fixed << setprecision(2);
        cout << "  Account No : " << id << "\n"
             << "  Holder     : " << holderName << "\n"
             << "  Type       : " << typeName() << "\n"
             << "  Balance    : " << balance << "\n";
    }
 
    // ---- common behaviour ----
    int getId() const { return id; }
    const string& getName() const { return holderName; }
    const string& getPin() const { return pin; }
    double getBalance() const { return balance; }
    const vector<Transaction>& getHistory() const { return history; }
    bool checkPin(const string& p) const { return pin == p; }
    void addLoadedTransaction(const Transaction& t) { history.push_back(t); }
 
    void deposit(double amount, const string& type = "Deposit",
                 const string& note = "Cash deposit") {
        balance += amount;
        record(type, amount, note);
    }
 
    bool withdraw(double amount, string& error, const string& type = "Withdraw",
                  const string& note = "Cash withdrawal") {
        if (amount <= 0) {
            error = "Amount must be positive.";
            return false;
        }
        if (amount > availableToWithdraw() + 1e-9) {
            ostringstream msg;
            msg << fixed << setprecision(2)
                << "Insufficient funds. Available to withdraw: " << availableToWithdraw();
            error = msg.str();
            return false;
        }
        balance -= amount;
        record(type, amount, note);
        return true;
    }
};
 
// ------------------------- Savings Account ------------------
class SavingsAccount : public Account {
public:
    static double minBalance() { return 100.0; }
    static double annualRate() { return 0.03; }  // 3%
 
    using Account::Account;
    string typeName() const override { return "Savings"; }
    double availableToWithdraw() const override {
        return max(0.0, balance - minBalance());
    }
    void applyMonthlyInterest() {
        double interest = balance * annualRate() / 12.0;
        if (interest > 0) deposit(round(interest * 100) / 100, "Interest", "Monthly interest");
    }
    void display() const override {
        Account::display();
        cout << "  Min Balance: " << minBalance() << "   Interest: "
             << annualRate() * 100 << "% per year\n";
    }
};
 
// ------------------------- Current Account ------------------
class CurrentAccount : public Account {
public:
    static double overdraftLimit() { return 1000.0; }
 
    using Account::Account;
    string typeName() const override { return "Current"; }
    double availableToWithdraw() const override { return balance + overdraftLimit(); }
    void display() const override {
        Account::display();
        cout << "  Overdraft  : up to " << overdraftLimit() << "\n";
    }
};
 
// ------------------------- Bank -----------------------------
class Bank {
private:
    vector<unique_ptr<Account>> accounts;
    int nextId = 1001;
    string fileName;
 
    unique_ptr<Account> makeAccount(const string& type, int id, const string& name,
                                    const string& pin, double balance) {
        if (type == "Savings") return unique_ptr<Account>(new SavingsAccount(id, name, pin, balance));
        if (type == "Current") return unique_ptr<Account>(new CurrentAccount(id, name, pin, balance));
        return nullptr;
    }
 
public:
    explicit Bank(string file) : fileName(move(file)) { load(); }
 
    Account* find(int id) {
        for (auto& a : accounts)
            if (a->getId() == id) return a.get();
        return nullptr;
    }
 
    Account* login(const string& action) {
        int id = readInt("Account number: ");
        Account* acc = find(id);
        if (!acc) {
            cout << "  Account not found.\n";
            return nullptr;
        }
        string pin = readPin("PIN: ");
        if (!acc->checkPin(pin)) {
            cout << "  Wrong PIN. Cannot " << action << ".\n";
            return nullptr;
        }
        return acc;
    }
 
    // ---- operations ----
    void createAccount() {
        cout << "\n--- Create New Account ---\n";
        string name = sanitize(readNonEmpty("Holder name: "));
        int choice;
        do {
            choice = readInt("Type (1 = Savings, 2 = Current): ");
        } while (choice != 1 && choice != 2);
        string pin = readPin("Choose a 4-digit PIN: ");
 
        double initial;
        if (choice == 1) {
            do {
                initial = readAmount("Initial deposit (min 100): ");
                if (initial < SavingsAccount::minBalance())
                    cout << "  Savings account needs at least 100.\n";
            } while (initial < SavingsAccount::minBalance());
        } else {
            initial = readAmount("Initial deposit: ");
        }
 
        int id = nextId++;
        auto acc = makeAccount(choice == 1 ? "Savings" : "Current", id, name, pin, 0.0);
        acc->deposit(initial, "Deposit", "Opening deposit");
        accounts.push_back(move(acc));
        save();
        cout << "  Account created successfully! Your account number is " << id << "\n";
    }
 
    void depositMoney() {
        cout << "\n--- Deposit ---\n";
        Account* acc = login("deposit");
        if (!acc) return;
        double amt = readAmount("Amount to deposit: ");
        acc->deposit(amt);
        save();
        cout << fixed << setprecision(2) << "  Done. New balance: " << acc->getBalance() << "\n";
    }
 
    void withdrawMoney() {
        cout << "\n--- Withdraw ---\n";
        Account* acc = login("withdraw");
        if (!acc) return;
        double amt = readAmount("Amount to withdraw: ");
        string err;
        if (acc->withdraw(amt, err)) {
            save();
            cout << fixed << setprecision(2) << "  Done. New balance: " << acc->getBalance() << "\n";
        } else {
            cout << "  Failed: " << err << "\n";
        }
    }
 
    void transferMoney() {
        cout << "\n--- Transfer ---\n";
        Account* from = login("transfer");
        if (!from) return;
        int toId = readInt("Destination account number: ");
        Account* to = find(toId);
        if (!to) {
            cout << "  Destination account not found.\n";
            return;
        }
        if (to == from) {
            cout << "  Cannot transfer to the same account.\n";
            return;
        }
        double amt = readAmount("Amount to transfer: ");
        string err;
        if (from->withdraw(amt, err, "Transfer Out", "To account " + to_string(toId))) {
            to->deposit(amt, "Transfer In", "From account " + to_string(from->getId()));
            save();
            cout << fixed << setprecision(2)
                 << "  Transfer successful. Your new balance: " << from->getBalance() << "\n";
        } else {
            cout << "  Failed: " << err << "\n";
        }
    }
 
    void showAccount() {
        cout << "\n--- Account Details ---\n";
        Account* acc = login("view details");
        if (!acc) return;
        acc->display();
    }
 
    void showHistory() {
        cout << "\n--- Transaction History ---\n";
        Account* acc = login("view history");
        if (!acc) return;
        const auto& h = acc->getHistory();
        if (h.empty()) {
            cout << "  No transactions yet.\n";
            return;
        }
        cout << fixed << setprecision(2);
        cout << left << setw(18) << "Date" << setw(14) << "Type" << right << setw(12)
             << "Amount" << setw(14) << "Balance" << "  Note\n";
        cout << string(76, '-') << "\n";
        for (const auto& t : h) {
            cout << left << setw(18) << t.date << setw(14) << t.type << right << setw(12)
                 << t.amount << setw(14) << t.balanceAfter << "  " << t.note << "\n";
        }
    }
 
    void listAllAccounts() const {
        cout << "\n--- All Accounts ---\n";
        if (accounts.empty()) {
            cout << "  No accounts yet.\n";
            return;
        }
        cout << fixed << setprecision(2);
        cout << left << setw(12) << "Acc No" << setw(25) << "Name" << setw(10) << "Type"
             << right << setw(14) << "Balance" << "\n";
        cout << string(61, '-') << "\n";
        for (const auto& a : accounts) {
            cout << left << setw(12) << a->getId() << setw(25) << a->getName() << setw(10)
                 << a->typeName() << right << setw(14) << a->getBalance() << "\n";
        }
    }
 
    void applyInterestToAll() {
        int count = 0;
        for (auto& a : accounts) {
            if (auto* s = dynamic_cast<SavingsAccount*>(a.get())) {
                s->applyMonthlyInterest();
                ++count;
            }
        }
        save();
        cout << "  Monthly interest applied to " << count << " savings account(s).\n";
    }
 
    void deleteAccount() {
        cout << "\n--- Delete Account ---\n";
        Account* acc = login("delete the account");
        if (!acc) return;
        string confirm = readLine("Type YES to confirm deletion: ");
        if (confirm != "YES") {
            cout << "  Cancelled.\n";
            return;
        }
        int id = acc->getId();
        accounts.erase(remove_if(accounts.begin(), accounts.end(),
                                 [id](const unique_ptr<Account>& a) { return a->getId() == id; }),
                       accounts.end());
        save();
        cout << "  Account " << id << " deleted.\n";
    }
 
    // ---- persistence ----
    void save() const {
        ofstream out(fileName);
        if (!out) {
            cout << "  Warning: could not save data to " << fileName << "\n";
            return;
        }
        out << fixed << setprecision(2);
        out << "NEXTID|" << nextId << "\n";
        for (const auto& a : accounts) {
            out << "ACC|" << a->typeName() << "|" << a->getId() << "|" << a->getName() << "|"
                << a->getPin() << "|" << a->getBalance() << "\n";
            for (const auto& t : a->getHistory()) {
                out << "TXN|" << a->getId() << "|" << t.date << "|" << t.type << "|" << t.amount
                    << "|" << t.balanceAfter << "|" << t.note << "\n";
            }
        }
    }
 
    void load() {
        ifstream in(fileName);
        if (!in) return;  // first run, no file yet
        string line;
        while (getline(in, line)) {
            auto p = split(line, '|');
            try {
                if (p.size() >= 2 && p[0] == "NEXTID") {
                    nextId = stoi(p[1]);
                } else if (p.size() >= 6 && p[0] == "ACC") {
                    auto acc = makeAccount(p[1], stoi(p[2]), p[3], p[4], stod(p[5]));
                    if (acc) accounts.push_back(move(acc));
                } else if (p.size() >= 6 && p[0] == "TXN") {
                    Account* acc = find(stoi(p[1]));
                    if (acc) {
                        string note = p.size() >= 7 ? p[6] : "";
                        acc->addLoadedTransaction({p[2], p[3], stod(p[4]), stod(p[5]), note});
                    }
                }
            } catch (...) {
                // skip corrupted line
            }
        }
    }
};
 
// ------------------------- Main -----------------------------
void printMenu() {
    cout << "\n=========== BANK MANAGEMENT SYSTEM ===========\n"
         << " 1. Create new account\n"
         << " 2. Deposit\n"
         << " 3. Withdraw\n"
         << " 4. Transfer\n"
         << " 5. Account details\n"
         << " 6. Transaction history\n"
         << " 7. List all accounts\n"
         << " 8. Apply monthly interest (Savings)\n"
         << " 9. Delete account\n"
         << " 0. Exit\n"
         << "==============================================\n";
}
 
int main() {
    Bank bank("bank_data.txt");
    while (true) {
        printMenu();
        int choice = readInt("Choose an option: ");
        switch (choice) {
            case 1: bank.createAccount(); break;
            case 2: bank.depositMoney(); break;
            case 3: bank.withdrawMoney(); break;
            case 4: bank.transferMoney(); break;
            case 5: bank.showAccount(); break;
            case 6: bank.showHistory(); break;
            case 7: bank.listAllAccounts(); break;
            case 8: bank.applyInterestToAll(); break;
            case 9: bank.deleteAccount(); break;
            case 0:
                cout << "Thank you for using the Bank Management System. Goodbye!\n";
                return 0;
            default: cout << "  Invalid option, try again.\n";
        }
    }
}
 
