#ifndef ARRAY_H
#define ARRAY_H

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

#endif // !ARRAY_H