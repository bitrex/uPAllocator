#ifndef ALLOCATOR_POLICY_H_INCLUDED
#define ALLOCATOR_POLICY_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <new>

#define ALLOCATOR_TRAITS(T)                \
typedef T                 type;            \
typedef type              value_type;      \
typedef value_type*       pointer;         \
typedef value_type const* const_pointer;   \
typedef value_type&       reference;       \
typedef value_type const& const_reference; \
typedef std::size_t       size_type;       \
typedef std::ptrdiff_t    difference_type; \


namespace uP_allocator {

template <typename T>
struct max_allocation {
  enum { value = static_cast<std::size_t>(sizeof(T) / sizeof(uint8_t)) };
};

template <typename T>
class uP_allocator_policy {
 public:
  ALLOCATOR_TRAITS(T)

  template <typename U>
  struct rebind {
    typedef uP_allocator_policy<U> other;
  };

  // ctr
  uP_allocator_policy(size_type num_blocks,
                      size_type size_of_each_block = max_allocation<T>::value)
      : _m_num_blocks(num_blocks),
        _m_size_of_each_block(size_of_each_block),
        _m_num_free_blocks(num_blocks),
        _m_num_initialized(0),
        _m_mem_start(reinterpret_cast<uintptr_t>(
            new uint8_t[_m_size_of_each_block * _m_num_blocks])),
        _m_next(_m_mem_start) {}

  // dtr
  ~uP_allocator_policy() {
    if (!std::is_integral<T>()) {
      DEBUG_MSG("Pool block of size: " << _m_size_of_each_block * _m_num_blocks
                                       << " deleted at address: "
                                       << reinterpret_cast<void*>(_m_mem_start)
                                       << "\n");

      operator delete[](reinterpret_cast<uint8_t*>(_m_mem_start));
    }
    _m_mem_start = reinterpret_cast<uintptr_t>(nullptr);
  }

  // copy ctr
  template <typename U>
  uP_allocator_policy(uP_allocator_policy<U> const& other) {}

  // Allocate memory
  pointer allocate(size_type count = 1, const_pointer /* hint */ = nullptr) {
    DEBUG_MSG("num blocks requested: " << count << "\n");
    DEBUG_MSG("num free blocks: " << _m_num_free_blocks << "\n");

    if (count > _m_num_free_blocks) {
      throw std::bad_alloc();
    }

    // get offset of current free block header from block to be allocated,
    // increment for allocation
    if (_m_num_initialized < _m_num_blocks) {
      auto p = reinterpret_cast<size_t*>(_addr_from_index(_m_num_initialized));
      *p = _m_num_initialized + count;
      _m_num_initialized += count;
    }

    auto ret = static_cast<pointer>(nullptr);

    // get address of next free block in pool
    if (0 < _m_num_free_blocks) {
      ret = reinterpret_cast<pointer>(_m_next);
      --_m_num_free_blocks;

      // get address of next free block header from block to be allocated,
      // store in _m_next
      if (0 != _m_num_free_blocks) {
        _m_next = _addr_from_index(*reinterpret_cast<size_t*>(_m_next));
      } else {
        _m_next = reinterpret_cast<uintptr_t>(static_cast<T*>(nullptr));
      }
    }
    DEBUG_MSG("One block of size: " << sizeof(*ret) * count
                                    << " allocated at address: " << ret
                                    << "\n");
    return ret;
  }

  // Delete memory
  void deallocate(pointer p, size_type count) {
    auto tmp = reinterpret_cast<size_t*>(&(*p));

    if (reinterpret_cast<size_t*>(_m_next) != nullptr) {
      *tmp = _index_from_addr(_m_next);
      _m_next = reinterpret_cast<uintptr_t>(&(*p));
      DEBUG_MSG("One block of size: "
                << sizeof(*p) << " deallocated at address: " << p << "\n");
    } else {
      *tmp = _m_num_blocks;
      _m_next = reinterpret_cast<uintptr_t>(&(*p));
    }
    ++_m_num_free_blocks;
    --count;

    if (0 == count) return;
    auto prev_ptr =
        reinterpret_cast<pointer>(_addr_from_index(_m_num_initialized - 1));
    deallocate(prev_ptr, count);
  }

 private:
  size_t _m_num_blocks;          // Num of blocks
  size_t _m_size_of_each_block;  // Size of each block
  size_t _m_num_free_blocks;     // Num of remaining blocks
  size_t _m_num_initialized;     // Num of initialized blocks
  uintptr_t _m_mem_start;        // Beginning of memory pool
  uintptr_t _m_next;             // Num of next free block

  uintptr_t _addr_from_index(size_type i) const {
    return _m_mem_start + (i * _m_size_of_each_block);
  }

  size_type _index_from_addr(uintptr_t p) const {
    return static_cast<size_type>(
        static_cast<std::ptrdiff_t>(p - _m_mem_start) / _m_size_of_each_block);
  }

  // Max number of bytes that can be held in a block
  size_type max_size() const { return max_allocation<T>::value; }
};
}

#endif  // UP_ALLOCATOR_POLICY_H_INCLUDED
