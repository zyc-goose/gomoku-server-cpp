#include <cstdio>
#include <cstring>
#include <queue>
#include "httplib.h"

class PatternMap {
public:
    const char *m_pattern;
    int m_score;
    int m_pattern_len;
    
    PatternMap(const char *pattern, int score) {
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
    PatternMap("bbbbb", 1000000)
};

class ACAutoNode {
public:
    int m_next[3] = {-1, -1, -1};
    int m_pattern_id = -1;
    int m_score_sum = 0;
    int m_fail = -1;
    int m_depth = 0;
};

int g_acauto_node_alloc_id = -1;
ACAutoNode g_acauto_nodes[100];

class ACAuto {
private:
    char id2ch[4] = "sbw";

    int ch2id(char ch) {
        if (ch == 's') return 0;
        if (ch == 'b') return 1;
        return 2;
    }

public:
    int m_root = -1;

    ACAuto() {
        g_acauto_node_alloc_id = -1;
        m_root = ++g_acauto_node_alloc_id;
        insertAll();
        build();
    }
    
    void insert(int pattern_id) {
        const char *word = g_pattern_maps[pattern_id].m_pattern;
        int word_len = strlen(word);
        int cur_node = m_root;
        for (int i = 0; i < word_len; ++i) {
            int ch_id = ch2id(word[i]);
            int next_node = g_acauto_nodes[cur_node].m_next[ch_id];
            if (next_node == -1) {
                next_node = ++g_acauto_node_alloc_id;
                g_acauto_nodes[cur_node].m_next[ch_id] = next_node;
                g_acauto_nodes[next_node].m_depth = i + 1;
            }
            cur_node = next_node;
        }
        g_acauto_nodes[cur_node].m_pattern_id = pattern_id;
    }

    void insertAll() {
        for (int i = 0; i < g_pattern_map_size; ++i) {
            insert(i);
        }
    }

    void build() {
        std::queue<int> q;
        q.push(m_root);
        while (!q.empty()) {
            int cur_node = q.front();
            q.pop();
            for (int i = 0; i < 3; ++i) {
                int next_node = g_acauto_nodes[cur_node].m_next[i];
                int fail_next_node;
                if (cur_node == m_root) {
                    fail_next_node = m_root;
                } else {
                    int fail_id = g_acauto_nodes[cur_node].m_fail;
                    fail_next_node = g_acauto_nodes[fail_id].m_next[i];
                }
                if (next_node == -1) {
                    g_acauto_nodes[cur_node].m_next[i] = fail_next_node;
                } else {
                    g_acauto_nodes[next_node].m_fail = fail_next_node;
                    q.push(next_node);
                }
            }
        }
    }

    int find_sum(const char s[], int pivot) {
        printf("ACAuto::find_sum searching: %s\n", s);
        int ret_sum = 0;
        int s_len = strlen(s);
        int cur_node = m_root;
        for (int i = 0; i < s_len; ++i) {
            int ch_id = ch2id(s[i]);
            cur_node = g_acauto_nodes[cur_node].m_next[ch_id];
            if (pivot <= i) {
                int tmp_node = cur_node;
                while (tmp_node != m_root) {
                    int tmp_depth = g_acauto_nodes[tmp_node].m_depth;
                    if (pivot < i - tmp_depth + 1 || tmp_depth < g_pattern_min_len) {
                        break;
                    }
                    int pattern_id = g_acauto_nodes[tmp_node].m_pattern_id;
                    if (pattern_id != -1) {
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

class GameStateObserver {
public:
    GameStateObserver(){}
    virtual ~GameStateObserver(){}
    virtual void willSet(int row, int col) = 0;
    virtual void didSet(int row, int col) = 0;
    virtual void didClear(int row, int col) = 0;
};

class GameState {
public:
    char m_state[15][16];
    int m_observer_alloc_id = -1;
    GameStateObserver *m_observers[5];

    GameState(const char repr[]) {
        load(repr);
    }

    void add_observer(GameStateObserver *observer) {
        m_observers[++m_observer_alloc_id] = observer;
    }

    void reset() {
        m_observer_alloc_id = -1;
        memset(m_observers, 0, sizeof(m_observers));
        memset(m_state, 's', sizeof(m_state));
        for (int i = 0; i < 15; ++i) {
            m_state[i][15] = 0;
        }
    }

    void load(const char repr[]) {
        reset();
        int repr_len = strlen(repr);
        int num = 0;
        char last_ch = 's';
        int p_row = 0, p_col = 0;
        for (int i = 0; i < repr_len; ++i) {
            char ch = repr[i];
            if (ch >= '0' && ch <= '9') {
                num *= 10;
                num += (ch - '0');
            } else {
                if (num == 0) {
                    num = 1;
                }
                for (int j = 0; j < num; ++j) {
                    m_state[p_row][p_col] = ch;
                    ++p_col;
                    if (p_col == 15) {
                        p_col = 0;
                        ++p_row;
                    }
                }
                num = 0;
            }
            last_ch = ch;
        }
    }

    void set(int row, int col, char ch) {
        for (int i = 0; i <= m_observer_alloc_id; ++i) {
            m_observers[i]->willSet(row, col);
        }
        m_state[row][col] = ch;
        for (int i = 0; i <= m_observer_alloc_id; ++i) {
            m_observers[i]->didSet(row, col);
        }
    }

    void clear(int row, int col) {
        m_state[row][col] = 's';
        for (int i = 0; i <= m_observer_alloc_id; ++i) {
            m_observers[i]->didClear(row, col);
        }
    }

    void print() {
        for (int i = 0; i < 15; ++i) {
            printf("%s\n", m_state[i]);
        }
    }
};

void inplace_invert(char s[]) {
    for (int i = 0; s[i]; ++i) {
        if (s[i] == 'b') {
            s[i] = 'w';
        } else if (s[i] == 'w') {
            s[i] = 'b';
        }
    }
}

int g_min(int a, int b) {
    return a < b ? a : b;
}

int g_min(int a, int b, int c) {
    return a < b ? g_min(a, c) : g_min(b, c);
}

int g_max(int a, int b) {
    return a > b ? a : b;
}

int g_max(int a, int b, int c) {
    return a > b ? g_max(a, c) : g_max(b, c);
}

class Evaluator: public GameStateObserver {
private:
    int m_stack_head = -1;
    int m_stack_before[20];
    int m_stack_after[20];
public:
    GameState *m_game_state;
    ACAuto *m_acauto;
    int m_score = 0;

    Evaluator(GameState &game_state, ACAuto &acauto) {
        m_game_state = &game_state;
        m_acauto = &acauto;
        m_game_state->add_observer(this);
    }

    ~Evaluator() {}

    int get_score_at(const int row, const int col) {
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
        for (int j = beg_col; j <= end_col; ++j) {
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
        for (int i = beg_row; i <= end_row; ++i) {
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
        for (int i = beg_row, j = beg_col; i <= end_row; ++i, ++j) {
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
        for (int i = beg_row, j = beg_col; j <= end_col; --i, ++j) {
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

    void willSet(int row, int col) {
        int score = get_score_at(row, col);
        ++m_stack_head;
        m_stack_before[m_stack_head] = score;
        m_score -= score;
        
        printf("Evaluator::willSet score = %d\n", m_score);
    }

    void didSet(int row, int col) {
        int score = get_score_at(row, col);
        m_stack_after[m_stack_head] = score;
        m_score += score;

        printf("Evaluator::didSet score = %d\n", m_score);
    }

    void didClear(int row, int col) {
        m_score -= m_stack_after[m_stack_head];
        m_score += m_stack_before[m_stack_head];
        --m_stack_head;

        printf("Evaluator::didClear score = %d\n", m_score);
    }
};

int main(int argc, char **argv) {
    using namespace httplib;

    printf("result: %d\n", g_acauto.find_sum("sbsbsbbbbsbbs", 9));

    GameState state("");
    Evaluator eval(state, g_acauto);
    state.print();
    state.set(12, 4, 'b');
    state.clear(12, 4);

    return 0;
}