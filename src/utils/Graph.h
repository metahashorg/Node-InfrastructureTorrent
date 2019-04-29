#ifndef GRAPH_H_
#define GRAPH_H_

#include <deque>
#include <vector>

template<typename T>
class Graph {
public:
    
    class Element {
    friend class Graph;
    public:
        
        Element(const T &element) 
            : element(element)
        {}
        
        const Element& getParent() const;
        
        bool isParent() const;
        
        const T& getElement() const;
        
    private:
        const T element;
        
        Element* parent = nullptr;
    };
    
public:
    
    void addEdge(const T &parent, const T &child);
    
    const Element& findElement(const T &element) const;
    
    std::vector<T> getAllElements() const;
    
private:
    
    Element& findOrAdd(const T &element);
    
private:
    
    std::deque<Element> elements;
};

#endif // GRAPH_H_
