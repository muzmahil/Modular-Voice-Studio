#pragma once
#include "ModuleProcessor.h"
#include <functional>
#include <vector>

/**
    Central registry of module types, grouped into categories. Add one
    registerType() call per module in ModuleFactory.cpp; the categorized
    "Add Module" submenu in NodeCanvas is built straight from this list,
    so nothing else needs to change.
*/
class ModuleFactory
{
public:
    using Creator = std::function<std::unique_ptr<ModuleProcessor>()>;

    static ModuleFactory& instance();

    void registerType (const juce::String& typeId, const juce::String& category, const juce::String& description, Creator creator);
    std::unique_ptr<ModuleProcessor> create (const juce::String& typeId) const;

    juce::StringArray getCategories() const;
    juce::StringArray getTypesInCategory (const juce::String& category) const;
    juce::String getDescription (const juce::String& typeId) const;
    juce::String getCategory (const juce::String& typeId) const;

private:
    ModuleFactory();

    struct Entry
    {
        juce::String typeId;
        juce::String category;
        juce::String description;
        Creator creator;
    };

    std::vector<Entry> entries;
};
