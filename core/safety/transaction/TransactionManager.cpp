#include "TransactionManager.h"
namespace polaris::safety {
domain::Transaction TransactionManager::create(const std::vector<std::string>&, bool) { return {}; }
bool TransactionManager::validate(const domain::Transaction&) { return true; }
bool TransactionManager::backup(const domain::Transaction&) { return true; }
bool TransactionManager::apply(const domain::Transaction&) { return false; }
bool TransactionManager::verify(const domain::Transaction&) { return true; }
bool TransactionManager::rollback(const std::string&) { return true; }
std::optional<domain::Transaction> TransactionManager::get(const std::string&) const { return std::nullopt; }
}
