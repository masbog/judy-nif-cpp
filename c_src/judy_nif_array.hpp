#ifndef JUDY_NIF_ARRAY_HPP
#define JUDY_NIF_ARRAY_HPP JUDY_NIF_ARRAY_HPP

// std::copy
#include <cstring>

// ERL_NIF_TERM, ErlNifBinary
#include <erl_nif.h>

// boost::pool, boost::object_pool
#include <boost/pool/poolfwd.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/object_pool.hpp>

// judy::hs
#include "judy-cpp.hpp"


namespace judy_nif {

typedef ErlNifBinary  binary_type;
typedef binary_type&  binary_reference;
typedef unsigned char binary_value_type;
typedef unsigned int  binary_size_type;

class binary_array
{
public:
    typedef binary_value_type key_type;
    typedef key_type& key_reference;
    typedef key_type* key_pointer;

    typedef binary_value_type value_type;
    typedef value_type& value_reference;
    typedef value_type* value_pointer;

    typedef binary_size_type size_type;


    typedef struct{
        size_type key_size;
        key_pointer key;

        size_type value_size;
        value_pointer value;
    } element_type;
    typedef element_type* element_pointer;


protected:
    typedef judy::hs<key_pointer, element_pointer> hs_array_type;
    typedef hs_array_type* hs_array_pointer;


    typedef boost::object_pool<
        element_type, boost::default_user_allocator_new_delete >
        element_allocator_type;

    typedef boost::pool<boost::default_user_allocator_malloc_free>
        binary_allocator_type;


public:
    binary_array(const std::size_t grow_by = 1024)
        : elem_alloc_(),
          binary_alloc_(sizeof(value_type), grow_by)
    {}


    bool
    insert(const binary_reference key0, const binary_reference value0)
    {
        const size_type value_size = value0.size;
        const size_type key_size   = key0.size;

        // Allocate memory for the new element.
        element_pointer elem = elem_alloc_.construct();
        // FIXME - Handle out-of-memory, value == 0.
        elem->value_size = value_size;
        elem->key_size = key_size;

        // Copy over the key.
        key_pointer key =
            static_cast<key_pointer>(binary_alloc_.ordered_malloc(key_size));
        // FIXME - Handle out-of-memory, value == 0.

        std::copy(key0.data, key0.data + key_size, key);
        elem->key = key;

        // And copy over the value.
        value_pointer value =
            static_cast<value_pointer>(
                binary_alloc_.ordered_malloc(value_size));
        // FIXME - Handle out-of-memory, value == 0.

        std::copy(value0.data, value0.data + value_size, value);
        elem->value = value;

        // Insert new element and retrieve old value (if any).
        element_pointer old_value =
            judy_.insert(elem->key, key_size, elem);

        // In case a value was present and thus replaced, free.
        if (old_value != 0) {
            binary_alloc_.ordered_free(old_value->key, old_value->key_size);
            binary_alloc_.ordered_free(old_value->value, old_value->value_size);
            elem_alloc_.destroy(old_value);
        }

        // Return whether the value was newly inserted.
        return old_value == 0;
    }


    ERL_NIF_TERM
    get(const binary_reference key, ErlNifEnv* env)
    {
        try {
            element_pointer elem = judy_.get(key.data, key.size);
            const size_type value_size = elem->value_size;

            ERL_NIF_TERM term;
            value_pointer binary = enif_make_new_binary(env, value_size, &term);
            std::copy(elem->value, (elem->value + value_size), binary);
            return term;
        }
        catch (bool) { return 0; }
    }


    bool
    remove(const binary_reference key)
    {
        element_pointer old_value = judy_.remove(key.data, key.size);

        // Return whether the value was actually removed.
        return old_value != 0;
    }


private:
    hs_array_type judy_;
    element_allocator_type elem_alloc_;
    binary_allocator_type binary_alloc_;
};

} // namespace

#endif
