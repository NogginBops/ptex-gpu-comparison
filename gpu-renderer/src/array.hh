#ifndef ARRAY_H
#define ARRAY_H

#include <assert.h>

#define alloc_array(type, size) (type*)malloc(size * sizeof(type))

template<typename T>
struct array_t {

    int size;
    int capacity;
    T* arr;

    array_t(int capacity)
    {
        this->size = 0;
        this->capacity = capacity;
        this->arr = alloc_array(T, capacity);
    }

    array_t(T* arr, int size)
    {
        this->size = size;
        this->capacity = size;
        this->arr = arr;
    }

    void add(T t)
    {
        ensureCapacity(size + 1);
        arr[size++] = t;
    }

    void clear()
    {
        size = 0;
    }

    void ensureCapacity(int capacity)
    {
        if (capacity >= this->capacity)
        {
            T* oldArr = arr;
            int new_cap = this->capacity + this->capacity / 2;
            if (new_cap < capacity) new_cap = capacity;
            T* newArr = alloc_array(T, new_cap);
            arr = newArr;
            memcpy(newArr, oldArr, size * sizeof(T));
            free(oldArr);
        }
    }

    T& operator[](int index)
    {
        return arr[index];
    }
};

template<typename T>
struct stack_t {

    int size;
    int capacity;
    T* arr;

    stack_t(int capacity)
    {
        this->size = 0;
        this->capacity = capacity;
        this->arr = alloc_array(T, capacity);
    }

    stack_t(T* arr, int size)
    {
        this->size = size;
        this->capacity = size;
        this->arr = arr;
    }

    void push(T t)
    {
        ensureCapacity(size + 1);
        arr[size++] = t;
    }

    T& pop() {
        assert(size > 0);
        return arr[--size];
    }

    T& peek()
    {
        if (size <= 0) throw std::runtime_error("Stack is empty");
        return arr[size - 1];
    }

    bool try_peek(T& val)
    {
        if (size <= 0) return false;
        val = arr[size - 1];
        return true;
    }

    void clear()
    {
        size = 0;
    }

    void ensureCapacity(int capacity)
    {
        if (capacity >= this->capacity)
        {
            T* oldArr = arr;
            int new_cap = this->capacity + this->capacity / 2;
            if (new_cap < capacity) new_cap = capacity;
            T* newArr = alloc_array(T, new_cap);
            arr = newArr;
            memcpy(newArr, oldArr, size * sizeof(T));
            free(oldArr);
        }
    }

    T& operator[](int index)
    {
        return arr[index];
    }
};


#endif // !ARRAY_H