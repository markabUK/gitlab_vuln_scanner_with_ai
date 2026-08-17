#pragma once

#include "../domain/Interfaces.hpp"
#include <vector>
#include <memory>
#include <string>
#include <iostream>

struct RoutedRegistry {
    std::shared_ptr<IMavenRegistry> registry;
    std::vector<std::string> prefixes;
};

class CompositeRegistry : public IMavenRegistry {
private:
    std::vector<RoutedRegistry> registries;

public:
    void AddRegistry(std::shared_ptr<IMavenRegistry> reg, const std::vector<std::string>& prefixes) {
        registries.push_back({reg, prefixes});
    }

    std::shared_ptr<IMavenRegistry> FindRegistryForGroup(const std::string& group) {
        for (const auto& routed : registries) {
            for (const auto& prefix : routed.prefixes) {
                if (prefix == "*" || group.find(prefix) == 0) { 
                    return routed.registry;
                }
            }
        }
        return nullptr;
    }

    std::optional<std::string> GetLatestVersion(const Dependency& dep) override {
        auto reg = FindRegistryForGroup(dep.group);
        if (reg) return reg->GetLatestVersion(dep);
        return std::nullopt;
    }

    DependencyChange InspectVersionDiff(const Dependency& oldDep, const Dependency& newDep) override {
        auto reg = FindRegistryForGroup(oldDep.group);
        if (reg) {
            return reg->InspectVersionDiff(oldDep, newDep);
        }
        
        // Safe fallback if no registry matched
        DependencyChange diff;
        diff.oldDep = oldDep;
        diff.newDep = newDep;
        diff.hasPackageMove = false;
        diff.releaseNotes = "";
        diff.skipAI = false;
        return diff;
    }
};