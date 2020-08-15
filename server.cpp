#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <queue>
#include <set>
#include <algorithm>
#include <thread>
#include <mutex>
#include "httplib.h"

class PatternMap
{
public:
    const char *m_pattern;
    int m_score;
    int m_pattern_len;

    PatternMap(const char *pattern, int score)
    {
        m_pattern = pattern;
        m_score = score;
        m_pattern_len = strlen(pattern);
    }
};

const int g_pattern_min_len = 4;
const int g_pattern_map_size = 18;
PatternMap g_pattern_maps[g_pattern_map_size] = {
    // 活2
    PatternMap("sbbs", 30),
    // 空3
    PatternMap("sbsbs", 20),
    // 半活3
    PatternMap("sbbbw", 50),
    PatternMap("wbbbs", 50),
    // 活3
    PatternMap("sbbbs", 1300),
    // 半空4
    PatternMap("sbsbbw", 500),
    PatternMap("wbsbbs", 500),
    PatternMap("sbbsbw", 500),
    PatternMap("wbbsbs", 500),
    // 空4
    PatternMap("sbsbbs", 1200),
    PatternMap("sbbsbs", 1200),
    // 半活4
    PatternMap("sbbbbw", 2100),
    PatternMap("wbbbbs", 2100),
    // 活4 (gg)
    PatternMap("sbbbbs", 100000),
    // 空5
    PatternMap("bsbbb", 1900),
    PatternMap("bbsbb", 1900),
    PatternMap("bbbsb", 1900),
    // 5连珠
    PatternMap("bbbbb", 1000000)};

class ACAutoNode
{
public:
    int m_next[3] = {-1, -1, -1};
    int m_pattern_id = -1;
    int m_score_sum = 0;
    int m_fail = -1;
    int m_depth = 0;
};

int g_acauto_node_alloc_id = -1;
ACAutoNode g_acauto_nodes[100];

class ACAuto
{
private:
    char id2ch[4] = "sbw";

    int ch2id(char ch)
    {
        if (ch == 's')
            return 0;
        if (ch == 'b')
            return 1;
        return 2;
    }

public:
    int m_root = -1;

    ACAuto()
    {
        g_acauto_node_alloc_id = -1;
        m_root = ++g_acauto_node_alloc_id;
        insertAll();
        build();
    }

    void insert(int pattern_id)
    {
        const char *word = g_pattern_maps[pattern_id].m_pattern;
        int word_len = strlen(word);
        int cur_node = m_root;
        for (int i = 0; i < word_len; ++i)
        {
            int ch_id = ch2id(word[i]);
            int next_node = g_acauto_nodes[cur_node].m_next[ch_id];
            if (next_node == -1)
            {
                next_node = ++g_acauto_node_alloc_id;
                g_acauto_nodes[cur_node].m_next[ch_id] = next_node;
                g_acauto_nodes[next_node].m_depth = i + 1;
            }
            cur_node = next_node;
        }
        g_acauto_nodes[cur_node].m_pattern_id = pattern_id;
    }

    void insertAll()
    {
        for (int i = 0; i < g_pattern_map_size; ++i)
        {
            insert(i);
        }
    }

    void build()
    {
        std::queue<int> q;
        q.push(m_root);
        while (!q.empty())
        {
            int cur_node = q.front();
            q.pop();
            for (int i = 0; i < 3; ++i)
            {
                int next_node = g_acauto_nodes[cur_node].m_next[i];
                int fail_next_node;
                if (cur_node == m_root)
                {
                    fail_next_node = m_root;
                }
                else
                {
                    int fail_id = g_acauto_nodes[cur_node].m_fail;
                    fail_next_node = g_acauto_nodes[fail_id].m_next[i];
                }
                if (next_node == -1)
                {
                    g_acauto_nodes[cur_node].m_next[i] = fail_next_node;
                }
                else
                {
                    g_acauto_nodes[next_node].m_fail = fail_next_node;
                    q.push(next_node);
                }
            }
        }
    }

    int find_sum(const char s[], int pivot)
    {
        int ret_sum = 0;
        int s_len = strlen(s);
        int cur_node = m_root;
        for (int i = 0; i < s_len; ++i)
        {
            int ch_id = ch2id(s[i]);
            cur_node = g_acauto_nodes[cur_node].m_next[ch_id];
            if (pivot <= i)
            {
                int tmp_node = cur_node;
                while (tmp_node != m_root)
                {
                    int tmp_depth = g_acauto_nodes[tmp_node].m_depth;
                    if (pivot < i - tmp_depth + 1 || tmp_depth < g_pattern_min_len)
                    {
                        break;
                    }
                    int pattern_id = g_acauto_nodes[tmp_node].m_pattern_id;
                    if (pattern_id != -1)
                    {
                        ret_sum += g_pattern_maps[pattern_id].m_score;
                    }
                    tmp_node = g_acauto_nodes[tmp_node].m_fail;
                }
            }
        }
        return ret_sum;
    }
};

ACAuto g_acauto;

class GameStateObserver
{
public:
    GameStateObserver() {}
    virtual ~GameStateObserver() {}
    virtual void willSet(int row, int col) = 0;
    virtual void didSet(int row, int col) = 0;
    virtual void didClear(int row, int col) = 0;
};

class GameState
{
public:
    char m_state[15][16];
    int m_observer_alloc_id = -1;
    GameStateObserver *m_observers[5];

    GameState(const char repr[])
    {
        load(repr);
    }

    void add_observer(GameStateObserver *observer)
    {
        m_observers[++m_observer_alloc_id] = observer;
    }

    void reset()
    {
        m_observer_alloc_id = -1;
        memset(m_observers, 0, sizeof(m_observers));
        memset(m_state, 's', sizeof(m_state));
        for (int i = 0; i < 15; ++i)
        {
            m_state[i][15] = 0;
        }
    }

    void load(const char repr[])
    {
        reset();
        int repr_len = strlen(repr);
        int num = 0;
        char last_ch = 's';
        int p_row = 0, p_col = 0;
        for (int i = 0; i < repr_len; ++i)
        {
            char ch = repr[i];
            if (ch >= '0' && ch <= '9')
            {
                num *= 10;
                num += (ch - '0');
            }
            else
            {
                if (num == 0)
                {
                    num = 1;
                }
                for (int j = 0; j < num; ++j)
                {
                    m_state[p_row][p_col] = ch;
                    ++p_col;
                    if (p_col == 15)
                    {
                        p_col = 0;
                        ++p_row;
                    }
                }
                num = 0;
            }
            last_ch = ch;
        }
    }

    void set(int row, int col, char ch)
    {
        for (int i = 0; i <= m_observer_alloc_id; ++i)
        {
            m_observers[i]->willSet(row, col);
        }
        m_state[row][col] = ch;
        for (int i = 0; i <= m_observer_alloc_id; ++i)
        {
            m_observers[i]->didSet(row, col);
        }
    }

    void clear(int row, int col)
    {
        m_state[row][col] = 's';
        for (int i = 0; i <= m_observer_alloc_id; ++i)
        {
            m_observers[i]->didClear(row, col);
        }
    }

    std::mutex m_mutex_print;

    void print()
    {
        m_mutex_print.lock();
        for (int i = 0; i < 15; ++i)
        {
            printf("%s\n", m_state[i]);
        }
        m_mutex_print.unlock();
    }

    void randomise()
    {
        for (int i = 0; i < 15; ++i)
        {
            for (int j = 0; j < 15; ++j)
            {
                int num = rand() % 3;
                char ch;
                if (num == 0)
                    ch = 's';
                else if (num == 1)
                    ch = 'b';
                else
                    ch = 'w';
                m_state[i][j] = ch;
            }
        }
    }
};

void inplace_invert(char s[])
{
    for (int i = 0; s[i]; ++i)
    {
        if (s[i] == 'b')
        {
            s[i] = 'w';
        }
        else if (s[i] == 'w')
        {
            s[i] = 'b';
        }
    }
}

int g_min(int a, int b)
{
    return a < b ? a : b;
}

int g_min(int a, int b, int c)
{
    return a < b ? g_min(a, c) : g_min(b, c);
}

int g_max(int a, int b)
{
    return a > b ? a : b;
}

int g_max(int a, int b, int c)
{
    return a > b ? g_max(a, c) : g_max(b, c);
}

class Evaluator : public GameStateObserver
{
private:
    int m_stack_head = -1;
    int m_stack_before[20];
    int m_stack_after[20];

    GameState *m_game_state;
    ACAuto *m_acauto;

public:
    int m_score = 0;

    Evaluator(GameState *game_state, ACAuto *acauto)
    {
        m_game_state = game_state;
        m_acauto = acauto;
        m_game_state->add_observer(this);
    }

    ~Evaluator() {}

    int get_str_pos(char s[], int si, int ds, int r, int c, int dr, int dc, int br, int bc, char block)
    {
        char first_ch = m_game_state->m_state[r][c];
        int first_si = si;
        s[si] = first_ch;
        r += dr;
        c += dc;
        si += ds;

        bool b_mark = false, w_mark = false;
        int space_pos = -1;
        while (true)
        {
            if (r == br || c == bc)
            { // out of bound (-1, 15)
                if (s[si - ds] == 's')
                {
                    return si - ds;
                }
                s[si] = block;
                return si;
            }
            char this_ch = m_game_state->m_state[r][c];
            s[si] = this_ch;
            if (this_ch == 's')
            {
                if (si == first_si + ds && (first_ch == 's' || first_ch == block))
                {
                    return first_si;
                }
                if (space_pos == -1)
                {
                    space_pos = si;
                }
                else if (space_pos + ds == si)
                {
                    return space_pos;
                }
                else
                {
                    return si;
                }
            }
            else if (this_ch == block)
            {
                if (first_ch == block || s[si - ds] == 's')
                {
                    return si - ds;
                }
                else
                {
                    return si;
                }
            }
            // update
            r += dr;
            c += dc;
            si += ds;
        }
        return -1;
    }

    int get_score_at(const int row, const int col)
    {
        int ret_score = 0;
        char s[22];
        int sh, st;
        const int sm = 10;

        // Horizontal:
        sh = get_str_pos(s, sm, -1, row, col, 0, -1, -1, -1, 'w');
        st = get_str_pos(s, sm, 1, row, col, 0, 1, 15, 15, 'w');
        if (st - sh >= 3)
        {
            s[st + 1] = 0;
            ret_score += m_acauto->find_sum(s + sh, sm - sh);
        }
        // white
        sh = get_str_pos(s, sm, -1, row, col, 0, -1, -1, -1, 'b');
        st = get_str_pos(s, sm, 1, row, col, 0, 1, 15, 15, 'b');
        if (st - sh >= 3)
        {
            s[st + 1] = 0;
            inplace_invert(s + sh);
            ret_score -= m_acauto->find_sum(s + sh, sm - sh);
        }

        // Vertical:
        sh = get_str_pos(s, sm, -1, row, col, -1, 0, -1, -1, 'w');
        st = get_str_pos(s, sm, 1, row, col, 1, 0, 15, 15, 'w');
        if (st - sh >= 3)
        {
            s[st + 1] = 0;
            ret_score += m_acauto->find_sum(s + sh, sm - sh);
        }
        // white
        sh = get_str_pos(s, sm, -1, row, col, -1, 0, -1, -1, 'b');
        st = get_str_pos(s, sm, 1, row, col, 1, 0, 15, 15, 'b');
        if (st - sh >= 3)
        {
            s[st + 1] = 0;
            inplace_invert(s + sh);
            ret_score -= m_acauto->find_sum(s + sh, sm - sh);
        }

        // Diagonal 1:
        sh = get_str_pos(s, sm, -1, row, col, -1, -1, -1, -1, 'w');
        st = get_str_pos(s, sm, 1, row, col, 1, 1, 15, 15, 'w');
        if (st - sh >= 3)
        {
            s[st + 1] = 0;
            ret_score += m_acauto->find_sum(s + sh, sm - sh);
        }
        // white
        sh = get_str_pos(s, sm, -1, row, col, -1, -1, -1, -1, 'b');
        st = get_str_pos(s, sm, 1, row, col, 1, 1, 15, 15, 'b');
        if (st - sh >= 3)
        {
            s[st + 1] = 0;
            inplace_invert(s + sh);
            ret_score -= m_acauto->find_sum(s + sh, sm - sh);
        }

        // Diagonal 2:
        sh = get_str_pos(s, sm, -1, row, col, 1, -1, 15, -1, 'w');
        st = get_str_pos(s, sm, 1, row, col, -1, 1, -1, 15, 'w');
        if (st - sh >= 3)
        {
            s[st + 1] = 0;
            ret_score += m_acauto->find_sum(s + sh, sm - sh);
        }
        // white
        sh = get_str_pos(s, sm, -1, row, col, 1, -1, 15, -1, 'b');
        st = get_str_pos(s, sm, 1, row, col, -1, 1, -1, 15, 'b');
        if (st - sh >= 3)
        {
            s[st + 1] = 0;
            inplace_invert(s + sh);
            ret_score -= m_acauto->find_sum(s + sh, sm - sh);
        }

        // FINALE
        return ret_score;
    }

    int get_score_at_deprecated(const int row, const int col)
    {
        int ret_score = 0;
        int beg_row, end_row, beg_col, end_col;
        int sid;
        char s[20];

        // movements:
        int l_move = g_min(5, col);
        int r_move = g_min(5, 14 - col);
        int t_move = g_min(5, row);
        int b_move = g_min(5, 14 - row);
        int tl_move = g_min(t_move, l_move);
        int tr_move = g_min(t_move, r_move);
        int bl_move = g_min(b_move, l_move);
        int br_move = g_min(b_move, r_move);

        // Horizontal: row fixed, col + 1
        beg_col = col - l_move;
        end_col = col + r_move;
        sid = -1;
        s[++sid] = 'w';
        for (int j = beg_col; j <= end_col; ++j)
        {
            s[++sid] = m_game_state->m_state[row][j];
        }
        s[++sid] = 'w';
        s[++sid] = 0;
        // black score
        ret_score += m_acauto->find_sum(s, l_move + 1);
        // white score
        s[0] = s[sid - 1] = 'b';
        inplace_invert(s);
        ret_score -= m_acauto->find_sum(s, l_move + 1);

        // Vertical: row + 1, col fixed
        beg_row = row - t_move;
        end_row = row + b_move;
        sid = -1;
        s[++sid] = 'w';
        for (int i = beg_row; i <= end_row; ++i)
        {
            s[++sid] = m_game_state->m_state[i][col];
        }
        s[++sid] = 'w';
        s[++sid] = 0;
        // black score
        ret_score += m_acauto->find_sum(s, t_move + 1);
        // white score
        s[0] = s[sid - 1] = 'b';
        inplace_invert(s);
        ret_score -= m_acauto->find_sum(s, t_move + 1);

        // Diagonal 1: row + 1, col + 1
        beg_row = row - tl_move;
        beg_col = col - tl_move;
        end_row = row + br_move;
        end_col = col + br_move;
        sid = -1;
        s[++sid] = 'w';
        for (int i = beg_row, j = beg_col; i <= end_row; ++i, ++j)
        {
            s[++sid] = m_game_state->m_state[i][j];
        }
        s[++sid] = 'w';
        s[++sid] = 0;
        // black score
        ret_score += m_acauto->find_sum(s, tl_move + 1);
        // white score
        s[0] = s[sid - 1] = 'b';
        inplace_invert(s);
        ret_score -= m_acauto->find_sum(s, tl_move + 1);

        // Diagonal 2: row - 1, col + 1
        beg_row = row + bl_move;
        beg_col = col - bl_move;
        end_row = row - tr_move;
        end_col = col + tr_move;
        sid = -1;
        s[++sid] = 'w';
        for (int i = beg_row, j = beg_col; j <= end_col; --i, ++j)
        {
            s[++sid] = m_game_state->m_state[i][j];
        }
        s[++sid] = 'w';
        s[++sid] = 0;
        // black score
        ret_score += m_acauto->find_sum(s, bl_move + 1);
        // white score
        s[0] = s[sid - 1] = 'b';
        inplace_invert(s);
        ret_score -= m_acauto->find_sum(s, bl_move + 1);

        // FINALE
        return ret_score;
    }

    void willSet(int row, int col)
    {
        int score = get_score_at(row, col);
        ++m_stack_head;
        m_stack_before[m_stack_head] = score;
        m_score -= score;
    }

    void didSet(int row, int col)
    {
        int score = get_score_at(row, col);
        m_stack_after[m_stack_head] = score;
        m_score += score;
    }

    void didClear(int row, int col)
    {
        m_score -= m_stack_after[m_stack_head];
        m_score += m_stack_before[m_stack_head];
        --m_stack_head;
    }
};

int p_encode(int row, int col)
{
    return (row << 4) | col;
}

int p_decode_row(int num)
{
    return num >> 4;
}

int p_decode_col(int num)
{
    return num & 15;
}

class Proximity : public GameStateObserver
{
private:
    GameState *m_game_state;

public:
    int m_stack_id = -1;
    int m_stack_row[225];
    int m_stack_col[225];

    int m_track_id = -1;
    int m_track_count[20];

    bool m_in_stack[15][15];

    Proximity(GameState *game_state)
    {
        m_game_state = game_state;
        m_game_state->add_observer(this);
        memset(m_in_stack, 0, sizeof(m_in_stack));
        for (int i = 0; i < 15; ++i)
        {
            for (int j = 0; j < 15; ++j)
            {
                if (m_game_state->m_state[i][j] != 's')
                {
                    search_at(i, j);
                }
            }
        }
    }

    int search_at(int row, int col)
    {
        int ret_count = 0;
        int rs = g_max(0, row - 2);
        int rt = g_min(14, row + 2);
        int cs = g_max(0, col - 2);
        int ct = g_min(14, col + 2);
        for (int i = rs; i <= rt; ++i)
        {
            for (int j = cs; j <= ct; ++j)
            {
                if (m_game_state->m_state[i][j] == 's' && !m_in_stack[i][j])
                {
                    ++ret_count;
                    ++m_stack_id;
                    m_stack_row[m_stack_id] = i;
                    m_stack_col[m_stack_id] = j;
                    m_in_stack[i][j] = true;
                }
            }
        }
        return ret_count;
    }

    void willSet(int row, int col) {}

    void didSet(int row, int col)
    {
        m_track_count[++m_track_id] = search_at(row, col);
    }

    void didClear(int row, int col)
    {
        int count = m_track_count[m_track_id--];
        for (int i = 0; i < count; ++i)
        {
            m_in_stack[m_stack_row[m_stack_id]][m_stack_col[m_stack_id]] = false;
            --m_stack_id;
        }
    }
};

class Monitor : public GameStateObserver
{
private:
    GameState *m_game_state;

    bool search_five(const int row, const int col, int dr, int dc, int sr, int sc, int tr, int tc)
    {
        char this_ch = m_game_state->m_state[row][col];
        if (this_ch == 's')
            return false;

        int r, c, ps, pt;
        for (r = row - dr, c = col - dc, ps = 0;
             r != sr && c != sc && m_game_state->m_state[r][c] == this_ch;
             r -= dr, c -= dc, --ps)
            ;

        for (r = row + dr, c = col + dc, pt = 0;
             r != tr && c != tc && m_game_state->m_state[r][c] == this_ch;
             r += dr, c += dc, ++pt)
            ;

        return pt - ps >= 4;
    }

    char search_single(int i, int j)
    {
        char ch = m_game_state->m_state[i][j];
        if (ch == 's')
            return 0;
        if (search_five(i, j, 1, 0, -1, -1, 15, 15) ||
            search_five(i, j, 0, 1, -1, -1, 15, 15) ||
            search_five(i, j, 1, 1, -1, -1, 15, 15) ||
            search_five(i, j, -1, 1, 15, -1, -1, 15))
            return ch;
        return 0;
    }

    char search_all()
    {
        for (int i = 0; i < 15; ++i)
        {
            for (int j = 0; j < 15; ++j)
            {
                char ch = search_single(i, j);
                if (ch != 0)
                {
                    return ch;
                }
            }
        }
        return 0;
    }

    int count_occurrence(char ch)
    {
        int ret = 0;
        for (int i = 0; i < 15; ++i)
            for (int j = 0; j < 15; ++j)
                if (m_game_state->m_state[i][j] == ch)
                    ++ret;
        return ret;
    }

public:
    char m_winner = 0;
    char m_next_ch = 0;
    bool m_black_next = false;
    bool m_is_empty = false;

    Monitor(GameState *game_state)
    {
        m_game_state = game_state;
        m_game_state->add_observer(this);
        m_winner = search_all();
        m_black_next = (count_occurrence('b') == count_occurrence('w'));
        m_next_ch = m_black_next ? 'b' : 'w';
        m_is_empty = (count_occurrence('s') == 225);
    }

    void willSet(int row, int col) {}

    void didSet(int row, int col)
    {
        m_black_next = !m_black_next;
        m_next_ch = m_black_next ? 'b' : 'w';
        m_winner = search_single(row, col);
    }

    void didClear(int row, int col)
    {
        m_black_next = !m_black_next;
        m_next_ch = m_black_next ? 'b' : 'w';
        m_winner = 0;
    }
};

class PointScoreCountTuple
{
public:
    int m_row = -1, m_col = -1, m_score = -1, m_node_count = 1;

    PointScoreCountTuple() {}

    PointScoreCountTuple(int row, int col, int score, int node_count)
    {
        m_row = row;
        m_col = col;
        m_score = score;
        m_node_count = node_count;
    }

    bool operator<(const PointScoreCountTuple &rhs) const
    {
        return m_score < rhs.m_score;
    }

    void print()
    {
        printf("{row = %d, col = %d, score = %d, nodeCount=%d}\n", m_row, m_col, m_score, m_node_count);
    }

    std::string to_json()
    {
        char s[200];
        sprintf(s, "{\"score\":%d,\"point\":{\"row\":%d,\"col\":%d},\"nodeCount\":%d}", m_score, m_row, m_col, m_node_count);
        return std::string(s);
    }
};

class PointSorter
{
private:
    GameState *m_game_state;
    Proximity *m_proximity;
    Evaluator *m_evaluator;
    Monitor *m_monitor;

public:
    int m_points_top = -1;
    PointScoreCountTuple m_points[225];

    PointSorter(GameState *game_state, Proximity *proximity, Evaluator *evaluator, Monitor *monitor)
    {
        m_game_state = game_state;
        m_proximity = proximity;
        m_evaluator = evaluator;
        m_monitor = monitor;
    }

    void sort()
    {
        // load 's' points
        m_points_top = -1;
        for (int i = 0; i <= m_proximity->m_stack_id; ++i)
        {
            int row = m_proximity->m_stack_row[i];
            int col = m_proximity->m_stack_col[i];
            if (m_game_state->m_state[row][col] == 's')
            {
                ++m_points_top;
                m_points[m_points_top].m_row = row;
                m_points[m_points_top].m_col = col;
                m_game_state->set(row, col, m_monitor->m_next_ch);
                m_points[m_points_top].m_score = m_evaluator->m_score;
                m_game_state->clear(row, col);
            }
        }
        std::sort(m_points, m_points + (m_points_top + 1));
    }
};

const int g_inf = 100000000;

std::mutex g_mutex_alpha, g_mutex_beta;
int g_alpha = -g_inf, g_beta = g_inf;

class MinimaxSingleThread
{
private:
    ACAuto *m_acauto;
    GameState *m_game_state;
    Proximity *m_proximity;
    Evaluator *m_evaluator;
    Monitor *m_monitor;
    PointSorter *m_point_sorter;

    int m_target_depth;

public:
    MinimaxSingleThread(const char repr[], int row, int col, int target_depth)
    {
        // setup
        m_acauto = &g_acauto;
        m_game_state = new GameState(repr);
        m_proximity = new Proximity(m_game_state);
        m_evaluator = new Evaluator(m_game_state, m_acauto);
        m_monitor = new Monitor(m_game_state);
        m_point_sorter = new PointSorter(m_game_state, m_proximity, m_evaluator, m_monitor);
        // first move
        m_game_state->set(row, col, m_monitor->m_next_ch);
        // depth
        m_target_depth = target_depth;
    }

    ~MinimaxSingleThread()
    {
        delete m_game_state;
        delete m_proximity;
        delete m_evaluator;
        delete m_monitor;
        delete m_point_sorter;
    }

    PointScoreCountTuple search(int depth, int alpha, int beta)
    {
        if (m_monitor->m_winner == 'b')
        {
            return PointScoreCountTuple(-1, -1, g_inf, 1);
        }
        if (m_monitor->m_winner == 'w')
        {
            return PointScoreCountTuple(-1, -1, -g_inf, 1);
        }
        if (depth == m_target_depth)
        {
            return PointScoreCountTuple(-1, -1, m_evaluator->m_score, 1);
        }

        m_point_sorter->sort(); // ascending order
        int cand_size = m_point_sorter->m_points_top + 1;
        std::vector<PointScoreCountTuple> cand_points, best_points;
        for (int i = 0; i < cand_size; ++i)
        {
            cand_points.push_back(m_point_sorter->m_points[i]);
        }
        int best_score;
        int node_count = 1;

        if (m_monitor->m_black_next)
        { // maximiser
            best_score = -g_inf;
            for (int i = cand_size - 1; i >= 0; --i)
            {
                auto point = cand_points[i];

                m_game_state->set(point.m_row, point.m_col, 'b');
                auto result = search(depth + 1, alpha, beta);
                cand_points[i].m_score = result.m_score;
                best_score = g_max(best_score, result.m_score);
                node_count += result.m_node_count;

                g_mutex_alpha.lock();
                alpha = g_max(alpha, g_alpha, best_score);
                g_mutex_alpha.unlock();

                m_game_state->clear(point.m_row, point.m_col);

                // alpha-beta pruning
                if (alpha >= beta)
                {
                    break;
                }
            }
            // update global beta
            if (depth == 0)
            {
                g_mutex_beta.lock();
                g_beta = g_min(g_beta, best_score);
                g_mutex_beta.unlock();
            }
        }
        else
        { // minimiser
            best_score = g_inf;
            for (int i = 0; i < cand_size; ++i)
            {
                auto point = cand_points[i];

                m_game_state->set(point.m_row, point.m_col, 'w');
                auto result = search(depth + 1, alpha, beta);
                cand_points[i].m_score = result.m_score;
                best_score = g_min(best_score, result.m_score);
                node_count += result.m_node_count;

                g_mutex_beta.lock();
                beta = g_min(beta, g_beta, best_score);
                g_mutex_beta.unlock();

                m_game_state->clear(point.m_row, point.m_col);

                // alpha-beta pruning
                if (alpha >= beta)
                {
                    break;
                }
            }
            // update global alpha
            if (depth == 0)
            {
                g_mutex_alpha.lock();
                g_alpha = g_max(g_alpha, best_score);
                g_mutex_alpha.unlock();
            }
        }
        for (auto &elem : cand_points)
        {
            if (elem.m_score == best_score)
            {
                best_points.push_back(elem);
            }
        }
        auto ret_point = best_points[rand() % best_points.size()];
        ret_point.m_node_count = node_count;
        return ret_point;
    }
};

class MinimaxMultithread
{
private:
    ACAuto *m_acauto;
    GameState *m_game_state;
    Proximity *m_proximity;
    Evaluator *m_evaluator;
    Monitor *m_monitor;
    PointSorter *m_point_sorter;

    int m_target_depth;
    std::vector<std::thread> m_threads;
    std::vector<PointScoreCountTuple> m_results;
    std::vector<MinimaxSingleThread *> m_minimax_list;

public:
    void thread_job(int tid)
    {
        auto result = m_minimax_list[tid]->search(0, -g_inf, g_inf);
        m_results[tid].m_score = result.m_score;
        m_results[tid].m_node_count = result.m_node_count;
    }

    MinimaxMultithread(const char repr[], int target_depth)
    {
        // setup
        m_acauto = &g_acauto;
        m_game_state = new GameState(repr);
        m_proximity = new Proximity(m_game_state);
        m_evaluator = new Evaluator(m_game_state, m_acauto);
        m_monitor = new Monitor(m_game_state);
        m_point_sorter = new PointSorter(m_game_state, m_proximity, m_evaluator, m_monitor);
        // depth
        m_target_depth = target_depth;

        // make threads
        m_point_sorter->sort();
        std::vector<PointScoreCountTuple> cand_points;
        if (m_monitor->m_black_next)
        {
            for (int i = m_point_sorter->m_points_top; i >= 0; --i)
            {
                cand_points.push_back(m_point_sorter->m_points[i]);
            }
        }
        else
        {
            for (int i = 0; i <= m_point_sorter->m_points_top; ++i)
            {
                cand_points.push_back(m_point_sorter->m_points[i]);
            }
        }
        for (auto &elem : cand_points)
        {
            int row = elem.m_row, col = elem.m_col;
            auto minimax = new MinimaxSingleThread(repr, row, col, target_depth - 1);
            m_minimax_list.push_back(minimax);
            m_results.push_back(elem);
        }
        for (int tid = 0; tid < cand_points.size(); ++tid)
        {
            m_threads.push_back(std::thread(&MinimaxMultithread::thread_job, std::ref(*this), tid));
        }
    }

    ~MinimaxMultithread()
    {
        delete m_game_state;
        delete m_proximity;
        delete m_evaluator;
        delete m_monitor;
        delete m_point_sorter;
        for (auto ptr : m_minimax_list)
        {
            delete ptr;
        }
    }

    PointScoreCountTuple run()
    {
        if (m_monitor->m_is_empty)
        {
            return PointScoreCountTuple(7, 7, 0, 0);
        }
        if (m_monitor->m_winner != 0)
        {
            return PointScoreCountTuple(-1, -1, 0, 0);
        }
        // threads
        for (auto &t : m_threads)
            t.join();
        // finish
        int best_score, node_count = 0;
        if (m_monitor->m_black_next)
        { // maximiser move
            best_score = -g_inf;
            for (auto &elem : m_results)
            {
                best_score = g_max(best_score, elem.m_score);
                node_count += elem.m_node_count;
            }
        }
        else
        { // minimiser move
            best_score = g_inf;
            for (auto &elem : m_results)
            {
                best_score = g_min(best_score, elem.m_score);
                node_count += elem.m_node_count;
            }
        }
        std::vector<PointScoreCountTuple> best_points;
        for (auto &elem : m_results)
        {
            if (elem.m_score == best_score)
            {
                best_points.push_back(elem);
            }
        }
        auto ret = best_points[rand() % best_points.size()];
        ret.m_node_count = node_count;
        return ret;
    }
};

int main(int argc, char **argv)
{
    using namespace httplib;

    srand(time(NULL));

    Server svr;

    svr.Get("/solve", [](const Request &req, Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        printf("Hit /solve\n");
        PointScoreCountTuple res_point;
        if (req.has_param("repr") && req.has_param("depth"))
        {
            printf("Received valid params\n");
            auto s_repr = req.get_param_value("repr");
            auto s_depth = req.get_param_value("depth");
            int depth = 3;
            sscanf(s_depth.c_str(), "%d", &depth);

            printf("repr = %s\n", s_repr.c_str());
            printf("depth = %d\n", depth);
            MinimaxMultithread minimax(s_repr.c_str(), depth);
            res_point = minimax.run();
        }
        res.set_content(res_point.to_json(), "application/json");
    });

    svr.listen("localhost", 8080);

    return 0;
}