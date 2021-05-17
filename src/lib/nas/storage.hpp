//
// This file is a part of UERANSIM open source project.
// Copyright (c) 2021 ALİ GÜNGÖR.
//
// The software and all associated files are licensed under GPL-3.0
// and subject to the terms and conditions defined in LICENSE file.
//

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <utils/common.hpp>

namespace nas
{

/*
 * - Items are unique, if already exists, deletes the previous one
 * - List have fixed size, if capacity is full, oldest item is deleted
 * - Automatically cleared after specified period
 * - The list is NOT thread safe
 */
template <typename T>
class NasList
{
  public:
    using backup_functor_type = std::function<void(const std::vector<T> &buffer, size_t count)>;

  private:
    const size_t m_sizeLimit;
    const int64_t m_autoClearingPeriod;
    const std::optional<backup_functor_type> m_backupFunctor;

    std::vector<T> m_data;
    size_t m_size;
    int64_t m_lastAutoCleared;

  public:
    NasList(size_t sizeLimit, int64_t autoClearingPeriod, std::optional<backup_functor_type> backupFunctor)
        : m_sizeLimit{sizeLimit}, m_autoClearingPeriod{autoClearingPeriod},
          m_backupFunctor{backupFunctor}, m_data{sizeLimit}, m_size{}, m_lastAutoCleared{::utils::CurrentTimeMillis()}
    {
    }

    NasList(const NasList &) = delete;
    NasList(NasList &&) = delete;

  public:
    void add(const T &item)
    {
        autoClearIfNecessary();
        remove(item);
        makeSlotForNewItem();

        m_data[m_size] = item;
        m_size++;

        touch();
    }

    void add(T &&item)
    {
        autoClearIfNecessary();
        remove(item);
        makeSlotForNewItem();

        m_data[m_size] = std::move(item);
        m_size++;

        touch();
    }

    void remove(const T &item)
    {
        autoClearIfNecessary();

        size_t index = ~0u;
        for (size_t i = 0; i < m_size; i++)
        {
            if (m_data[i] == item)
            {
                index = i;
                break;
            }
        }

        if (index != ~0u)
            removeAt(index);
    }

    bool contains(const T &item)
    {
        autoClearIfNecessary();

        for (size_t i = 0; i < m_size; i++)
            if (m_data[i] == item)
                return true;
        return false;
    }

    template <typename Functor>
    void forEach(Functor &&fun)
    {
        autoClearIfNecessary();

        for (size_t i = 0; i < m_size; i++)
            fun((const T &)m_data[i]);
    }

    template <typename Functor>
    void mutateForEach(Functor &&fun)
    {
        autoClearIfNecessary();

        for (size_t i = 0; i < m_size; i++)
            fun(m_data[i]);

        touch();
    }

    void clear()
    {
        int64_t currentTime = ::utils::CurrentTimeMillis();
        if (currentTime - m_lastAutoCleared >= m_autoClearingPeriod)
            m_lastAutoCleared = currentTime;

        m_data.clear();
        m_size = 0;

        touch();
    }

    [[nodiscard]] size_t size() const
    {
        autoClearIfNecessary();

        return m_data.size();
    }

  private:
    void autoClearIfNecessary()
    {
        int64_t currentTime = ::utils::CurrentTimeMillis();
        if (currentTime - m_lastAutoCleared >= m_autoClearingPeriod)
        {
            m_lastAutoCleared = currentTime;
            clear();
        }
    }

    void makeSlotForNewItem()
    {
        if (m_size >= m_sizeLimit)
            removeAt(0);
    }

    void removeAt(size_t index)
    {
        touch();

        for (size_t i = index; i < m_size; ++i)
            m_data[i] = i + 1 < m_sizeLimit ? m_data[i + 1] : T{};
        m_size--;
    }

    void touch()
    {
        if (m_backupFunctor)
            (*m_backupFunctor)(m_data, m_size);
    }
};

} // namespace nas
