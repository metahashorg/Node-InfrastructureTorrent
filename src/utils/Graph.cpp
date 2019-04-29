#include "Graph.h"

#include <algorithm>
#include <functional>

#include "check.h"

using namespace common;

template<typename T>
typename Graph<T>::Element& Graph<T>::findOrAdd(const T& element) {
    auto found = std::find_if(elements.begin(), elements.end(), [&element](const Element &el) {return el.element == element;});
    if (found != elements.end()) {
        return *found;
    }
    return elements.emplace_back(element);
}

template<typename T>
void Graph<T>::addEdge(const T& parent, const T& child) {
    Element &first = findOrAdd(parent);
    Element &second = findOrAdd(child);
    CHECK(second.parent == nullptr, "Child " + child + " already has a parent");
    second.parent = &first;
}

template<typename T>
const typename Graph<T>::Element& Graph<T>::findElement(const T& element) const {
    auto found = std::find_if(elements.begin(), elements.end(), [&element](const Element &el) {return el.element == element;});
    CHECK(found != elements.end(), "Element " + element + " not found in graph");
    return *found;
}

template<typename T> 
bool Graph<T>::Element::isParent() const {
    return parent != nullptr;
}

template<typename T>
const typename Graph<T>::Element& Graph<T>::Element::getParent() const {
    CHECK(isParent(), "Not parent in element " + element);
    return *parent;
}

template<typename T>
const T& Graph<T>::Element::getElement() const {
    return element;
}

template<typename T>
std::vector<T> Graph<T>::getAllElements() const {
    std::vector<T> result;
    result.reserve(elements.size());
    std::transform(elements.begin(), elements.end(), std::back_inserter(result), std::mem_fn(&Graph<T>::Element::element));
    return result;
}

template class Graph<std::string>;
