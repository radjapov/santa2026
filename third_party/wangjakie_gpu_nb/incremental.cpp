#include <cmath>

#include <set>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using Value = int64_t;
struct Recipe { char op; Value a, b; };
struct Inverses {
    std::array<Recipe, 6> entries;
    int size = 0;
    void push_back(Recipe recipe) { entries[size++] = recipe; }
    const Recipe* begin() const { return entries.data(); }
    const Recipe* end() const { return entries.data() + size; }
};
struct Command { char op; int a, b, dst; };
using Tape = std::vector<Command>;
using Graph = std::unordered_map<Value, Recipe>;

struct Puzzle {
    Value limit;
    int registers;
    std::vector<Value> starts, targets;
    int rounds, max_neighbors;
    std::mt19937 rng;
    std::vector<Value> reference_order;

    void remember_order(const Tape& tape) {
        reference_order.clear();
        std::vector<Value> contents(registers);
        std::copy(starts.begin(),starts.end(),contents.begin());
        std::unordered_set<Value> seen(starts.begin(),starts.end());
        for(auto ins:tape){
            Value v=calculate(ins.op,contents[ins.a],contents[ins.b]);
            contents[ins.dst]=v;
            if(seen.insert(v).second)reference_order.push_back(v);
        }
    }

    bool guided_order(const Graph& graph,const std::vector<Value>& needed,Tape& tape) const {
        if(reference_order.empty())return false;
        std::unordered_set<Value> used(needed.begin(),needed.end());
        std::vector<Value> roots,order;
        roots.reserve(needed.size());
        for(Value v:reference_order)if(used.erase(v))roots.push_back(v);
        for(Value v:needed)if(used.erase(v))roots.push_back(v);
        return order_for(graph,roots,order)&&allocate(graph,order,tape);
    }

    Value calculate(char op, Value a, Value b) const {
        switch (op) {
        case '+': return a <= limit - b ? a + b : 0;
        case '-': return a > b ? a - b : 0;
        case '*': return a <= limit / b ? a * b : 0;
        case '/': return a % b == 0 ? a / b : 0;
        }
        return 0;
    }

    Graph graph_from(const Tape& tape) const {
        Graph graph;
        std::vector<Value> registers_now(registers);
        for (size_t i = 0; i < starts.size(); ++i) {
            registers_now[i] = starts[i];
            graph.emplace(starts[i], Recipe{0, 0, 0});
        }
        for (auto [op, a, b, dst] : tape) {
            Value left = registers_now[a], right = registers_now[b];
            if (!left || !right) return {};
            Value value = calculate(op, left, right);
            if (!value) return {};
            registers_now[dst] = value;
            graph.emplace(value, Recipe{op, left, right});
        }
        return graph;
    }

    bool visit(Value value, const Graph& graph,
               std::unordered_map<Value, int>& state, std::vector<Value>& order) const {
        auto [mark, fresh] = state.emplace(value, 1);
        if (!fresh) return mark->second == 2;
        auto found = graph.find(value);
        if (found == graph.end()) return false;
        auto [op, a, b] = found->second;
        if (op) {
            if (!visit(a, graph, state, order) || !visit(b, graph, state, order)) return false;
            order.push_back(value);
        }
        state[value] = 2;
        return true;
    }

    bool order_for(const Graph& graph, const std::vector<Value>& roots,
                   std::vector<Value>& order) const {
        std::unordered_map<Value, int> state;
        state.reserve(graph.size());
        order.clear();
        for (Value value : roots)
            if (!visit(value, graph, state, order)) return false;
        return true;
    }

    bool allocate(const Graph& graph, const std::vector<Value>& order, Tape& tape) const {
        std::unordered_map<Value, int> uses, slots;
        std::vector<int> free_slots;
        tape.clear();
        for (Value value : order) {
            auto [op, a, b] = graph.at(value);
            ++uses[a];
            ++uses[b];
        }
        for (int i = registers - 1; i >= 0; --i) {
            if (i >= int(starts.size()) || !uses.count(starts[i]) || slots.count(starts[i]))
                free_slots.push_back(i);
            else
                slots[starts[i]] = i;
        }
        for (Value value : order) {
            auto [op, a, b] = graph.at(value);
            int left = slots.at(a), right = slots.at(b);
            if (--uses[a] == 0) free_slots.push_back(left);
            if (--uses[b] == 0) free_slots.push_back(right);
            if (free_slots.empty()) return false;
            int dst = free_slots.back();
            tape.push_back({op, left, right, dst});
            if (uses.count(value)) {
                slots[value] = dst;
                free_slots.pop_back();
            }
        }
        return true;
    }

    bool pressure_order(const Graph& graph, std::vector<Value>& order) const {
        std::unordered_map<Value, int> pending, remaining;
        std::unordered_map<Value, std::vector<Value>> consumers;
        std::vector<Value> ready, scheduled;
        for (Value value : order) {
            auto [op, a, b] = graph.at(value);
            consumers[a].push_back(value);
            consumers[b].push_back(value);
            pending[value] = int(graph.at(a).op != 0) + int(graph.at(b).op != 0);
            if (!pending[value]) ready.push_back(value);
        }
        for (const auto& [value, uses] : consumers) remaining[value] = int(uses.size());
        while (!ready.empty()) {
            auto delta = [&](Value value) {
                auto [op, a, b] = graph.at(value);
                int releases = a == b ? int(remaining[a] == 2)
                                      : int(remaining[a] == 1) + int(remaining[b] == 1);
                return int(consumers.count(value)) - releases;
            };
            auto chosen = std::min_element(ready.begin(), ready.end(),
                                           [&](Value a, Value b) { return delta(a) < delta(b); });
            Value value = *chosen;
            ready.erase(chosen);
            scheduled.push_back(value);
            auto [op, a, b] = graph.at(value);
            --remaining[a];
            --remaining[b];
            auto found = consumers.find(value);
            if (found != consumers.end())
                for (Value next : found->second)
                    if (--pending[next] == 0) ready.push_back(next);
        }
        if (scheduled.size() != order.size()) return false;
        order.swap(scheduled);
        return true;
    }

    bool compile(const Graph& graph, Tape& tape) {
        auto roots = targets;
        std::vector<Value> order;
        if (!order_for(graph, roots, order)) return false;
        if (allocate(graph, order, tape)) return true;
        if (guided_order(graph,order,tape)) return true;
        if (pressure_order(graph, order) && allocate(graph, order, tape)) return true;
        std::reverse(roots.begin(), roots.end());
        for (int attempt = 0; attempt < 18; ++attempt) {
            if (order_for(graph, roots, order) && allocate(graph, order, tape)) return true;
            std::shuffle(roots.begin(), roots.end(), rng);
        }
        return false;
    }

    Inverses inverses(Value value, Value other) const {
        Inverses recipes;
        if (value > other) recipes.push_back({'+', other, value - other});
        if (value <= limit - other) recipes.push_back({'-', value + other, other});
        if (other > value) recipes.push_back({'-', other, other - value});
        if (value % other == 0) recipes.push_back({'*', other, value / other});
        if (value <= limit / other) recipes.push_back({'/', value * other, other});
        if (other % value == 0) recipes.push_back({'/', other, other / value});
        return recipes;
    }

    Graph live_only(const Graph& graph, const std::vector<Value>& order) const {
        Graph live;
        live.reserve(order.size() + starts.size());
        for (Value value : starts) live.emplace(value, Recipe{0, 0, 0});
        for (Value value : order) live.emplace(value, graph.at(value));
        return live;
    }


    struct Edge { int a, b; char op; };
    struct Via { int helper, other; Recipe step; };
    using IdList = std::vector<int>;
    bool search_group(Graph& graph, Tape& best) {
        const bool fork_repair = max_neighbors == 18;
        const bool downstream = max_neighbors == 17 || fork_repair;
        const bool shared_chain = max_neighbors == 19;
        const bool extended_groups = max_neighbors >= 24;
        const bool all_pairs = max_neighbors >= 40;
        const bool two_helpers = extended_groups && !all_pairs;
        std::vector<Value> sorted;
        if (!order_for(graph, targets, sorted)) return false;
        graph = live_only(graph, sorted);
        std::vector<Value> values = starts;
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        int initial = (int)values.size();
        values.insert(values.end(), sorted.begin(), sorted.end());
        int n = (int)values.size();
        std::unordered_map<Value,int> index, mid_index;
        for (int i=0;i<n;i++) index[values[i]]=i;
        std::vector<std::vector<Edge>> direct(n), makers;
        std::vector<IdList> users(n);
        std::vector<Value> helpers;
        std::vector<bool> target(n);
        for (auto v:targets) target[index.at(v)]=true;
        for (int i=initial;i<n;i++) {
            auto [op,a,b]=graph.at(values[i]);
            int ia=index.at(a), ib=index.at(b);
            direct[i].push_back({ia,ib,op});
            users[ia].push_back(i);
            if (ia!=ib) users[ib].push_back(i);
        }
        for (int i=0;i<n;i++) for(int j=0;j<=i;j++) {
            int a=values[i]>=values[j]?i:j,b=i+j-a;
            for (char op: {'+','-','*','/'}) {
                Value out=calculate(op,values[a],values[b]);
                if (!out || out==values[a] || out==values[b]) continue;
                auto f=index.find(out);
                if (f!=index.end()) {
                    if(f->second>=initial) direct[f->second].push_back({a,b,op});
                } else {
                    auto [it,fresh]=mid_index.emplace(out,(int)helpers.size());
                    if(fresh) {helpers.push_back(out); makers.emplace_back();}
                    makers[it->second].push_back({a,b,op});
                }
            }
        }
        std::vector<std::vector<Via>> via(n);
        std::vector<std::vector<std::pair<int,Via>>> uses_mid(helpers.size());
        for(int v=initial;v<n;v++) for(int f=0;f<n;f++) {
            if(v==f) continue;
            for(auto r:inverses(values[v],values[f])) {
                Value h=r.a==values[f]?r.b:r.a;
                auto it=mid_index.find(h);
                if(it!=mid_index.end()) {
                    Via use{it->second,f,r};
                    via[v].push_back(use);
                    uses_mid[use.helper].push_back({v,use});
                }
            }
        }
        std::set<IdList> unique;
        auto add_group=[&](IdList g) {
            std::sort(g.begin(),g.end());
            g.erase(std::unique(g.begin(),g.end()),g.end());
            if(g.empty() || g.front()<initial) return;
            for(int i:g) if(target[i]) return;
            unique.insert(g);
        };
        for(int i=initial;i<n;i++) {
            auto e=direct[i][0];
            add_group({i}); add_group({e.a,e.b});
            add_group({i,e.a}); add_group({i,e.b}); add_group({i,e.a,e.b});
            for(auto v:users[i]) for(auto w:users[i]) if(v<w) {
                add_group({v,w}); add_group({i,v,w});
            }
            for(auto v:users[i]) for(auto w:users[v]) add_group({i,v,w});
            if(extended_groups) {
                for(auto v:users[i]) {
                    for(auto w:users[v]) {
                        for(auto x:users[w]) add_group({i,v,w,x});
                        for(auto x:users[i]) add_group({i,x,v,w});
                        auto p=direct[w][0]; add_group({i,v,p.a,p.b});
                    }
                    auto p=direct[v][0]; add_group({e.a,e.b,p.a,p.b});
                }
                if(all_pairs) for(int j=initial;j<i;j++) add_group({i,j});
            }
        }
        std::vector<IdList> groups(unique.begin(),unique.end());
        std::shuffle(groups.begin(),groups.end(),rng);
        std::vector<int> stamp(helpers.size(),-1), weight(helpers.size()), last_use(helpers.size());
        std::vector<unsigned char> banned(n);
        std::vector<Recipe> chosen(n);
        auto propagate=[&](std::vector<unsigned char>& seen,std::vector<Recipe>& pick,int h,int second=-1) {
            bool more=true;
            while(more) {
                more=false;
                for(int v=initial;v<n;v++) if(!seen[v] && !banned[v]) {
                    for (auto e:direct[v]) if(seen[e.a] && seen[e.b]) {
                        seen[v]=1; pick[v]={e.op,values[e.a],values[e.b]}; more=true;break;
                    }
                }
                if(h>=0) for(auto& [v,u]:uses_mid[h]) if(!seen[v] && !banned[v] && seen[u.other]) {
                    seen[v]=1;pick[v]=u.step;more=true;
                }
                if(second>=0) for(auto& [v,u]:uses_mid[second]) if(!seen[v] && !banned[v] && seen[u.other]) {
                    seen[v]=1;pick[v]=u.step;more=true;
                }
            }
        };
        auto satisfied=[&](const auto& seen) {
            for(int v=initial;v<n;v++) if(target[v] && !seen[v]) return false;
            return true;
        };
        auto accept=[&](const auto& seen, const auto& pick,int h,Edge maker,int h2=-1,Edge maker2={}) {
            Graph trial;
            for(int v=0;v<n;v++) if(seen[v]) trial.emplace(values[v],pick[v]);
            if(h>=0) trial.emplace(helpers[h],Recipe{maker.op,values[maker.a],values[maker.b]});
            if(h2>=0) trial.emplace(helpers[h2],Recipe{maker2.op,values[maker2.a],values[maker2.b]});
            std::vector<Value> order;
            Tape candidate;
            if(!order_for(trial,targets,order) || order.size()>=best.size() || !compile(trial,candidate)) return false;
            graph=live_only(trial,order); best=std::move(candidate);return true;
        };
        for(int attempt=0;attempt<(int)groups.size();attempt++) {
            const auto& holes=groups[attempt];
            if((downstream) && holes.size()!=1) continue;
            std::fill(banned.begin(),banned.end(),0);
            for(int v:holes) banned[v]=1;
            std::vector<unsigned char> seen(n);
            std::fill_n(seen.begin(),initial,1);
            propagate(seen,chosen,-1);
            if(satisfied(seen)) {
                if(accept(seen,chosen,-1,{})) return true;
                continue;
            }
            if(downstream) {
                IdList available;
                for(int v=0;v<n;v++) if(seen[v]) available.push_back(v);
                if(available.size()<2) continue;
                std::vector<int> maker_id(helpers.size(),-2);
                auto get_maker=[&](int h) {
                    auto& k=maker_id[h];
                    if(k==-2) {
                        k=-1;
                        for(int i=0;i<(int)makers[h].size();i++) {
                            auto e=makers[h][i];
                            if(seen[e.a]&&seen[e.b]) {k=i;break;}
                        }
                    }
                    return k;
                };
                for(int v=initial;v<n;v++) {
                    if(seen[v] || banned[v]) continue;
                    auto free_seen=seen;
                    auto free_pick=chosen;
                    free_seen[v]=1;
                    free_pick[v]={0,0,0};
                    propagate(free_seen,free_pick,-1);
                    if(!satisfied(free_seen)) continue;
                    Graph trial;
                    for(int k=0;k<n;k++) if(free_seen[k]) trial.emplace(values[k],free_pick[k]);
                    std::vector<Value> without;
                    if(!order_for(trial,targets,without)) continue;
                    int room=(int)best.size()-1-(int)without.size();
                    if(room<2) continue;
                    std::unordered_set<Value> cheap(without.begin(),without.end());
                    cheap.insert(starts.begin(),starts.end());
                    auto quick=[&](Recipe end,Value u,Recipe ru,Value w=0,Recipe rw=Recipe{},Value z=0,Recipe rz=Recipe{}) {
                        int toll=2+(w!=0)+(z!=0);
                        std::array<Value,8> inputs={end.a,end.b,ru.a,ru.b,rw.a,rw.b,rz.a,rz.b};
                        std::sort(inputs.begin(),inputs.end());
                        Value last=-1;
                        for(Value x:inputs) if(x && x!=last && x!=u && x!=w && x!=z) {
                            last=x;
                            if(!cheap.count(x)) toll++;
                        }
                        if(toll>room || (w&&w==u) || (z&&(z==u||z==w)) || u==values[v] || w==values[v] || z==values[v]) return false;
                        trial[values[v]]=end;trial[u]=ru;if(w) trial[w]=rw;if(z) trial[z]=rz;
                        std::vector<Value> order;
                        Tape tape;
                        bool ok=order_for(trial,targets,order)&&order.size()<best.size()&&compile(trial,tape);
                        if(ok) {graph=live_only(trial,order);best=std::move(tape);}
                        trial.erase(u);if(w) trial.erase(w);if(z) trial.erase(z);
                        return ok;
                    };
                    auto origin=[&](int h,int k) {
                        auto e=makers[h][k];return Recipe{e.op,values[e.a],values[e.b]};
                    };
                    for(auto& u:via[v]) if(seen[u.other]) {
                        int k=get_maker(u.helper);
                        if(k>=0&&quick(u.step,helpers[u.helper],origin(u.helper,k))) return true;
                    }
                    if(room<3) continue;
                    for(int f:available) for(auto end:inverses(values[v],values[f])) {
                        Value w=end.a==values[f]?end.b:end.a;
                        if(index.count(w)) continue;
                        for(int j:available) for(auto rw:inverses(w,values[j])) {
                            Value u=rw.a==values[j]?rw.b:rw.a;
                            auto it=mid_index.find(u);
                            if(it==mid_index.end()) continue;
                            int k=get_maker(it->second);
                            if(k>=0 && quick(end,u,origin(it->second,k),w,rw)) return true;
                        }
                    }
                    for(int h=0;h<(int)helpers.size();h++) {
                        int k=get_maker(h);if(k<0) continue;
                        Value u=helpers[h];
                        for(auto end:inverses(values[v],u)) {
                            Value w=end.a==u?end.b:end.a;
                            auto it=mid_index.find(w);
                            if(it==mid_index.end() || it->second<=h) continue;
                            int j=get_maker(it->second);
                            if(j>=0&&quick(end,u,origin(h,k),w,origin(it->second,j))) return true;
                        }
                    }
                    if(!fork_repair || room<4) continue;
                    // In a four-step repair with exactly four spare steps, an
                    // operand outside the retained graph already exceeds the budget.
                    IdList mids;
                    for(int h=0;h<(int)helpers.size();h++) {
                        int k=get_maker(h);if(k<0) continue;
                        auto e=makers[h][k];
                        if(room>4 || (cheap.count(values[e.a])&&cheap.count(values[e.b]))) mids.push_back(h);
                    }
                    auto small=[&](int a,int b){return helpers[a]<helpers[b];};
                    int breadth=std::min<int>(360,mids.size());
                    std::partial_sort(mids.begin(),mids.begin()+breadth,mids.end(),small);
                    mids.resize(breadth);
                    for(int f:available) {
                        if(room==4&&!cheap.count(values[f])) continue;
                        for(auto end:inverses(values[v],values[f])) {
                            Value z=end.a==values[f]?end.b:end.a;
                            if(index.count(z)) continue;
                            for(int h:mids) {
                                Value u=helpers[h];
                                for(auto rz:inverses(z,u)) {
                                    Value w=rz.a==u?rz.b:rz.a;
                                    auto j=mid_index.find(w);
                                    if(j==mid_index.end()) continue;
                                    int mk=get_maker(j->second);
                                    if(mk>=0&&quick(end,u,origin(h,get_maker(h)),w,origin(j->second,mk),z,rz)) return true;
                                }
                            }
                        }
                    }
                }
                continue;
            }
            if(holes.size()<2 && !extended_groups && !shared_chain) continue;
            IdList frontier;
            for(int v:holes) for(int w:users[v]) if(!seen[w]&&!banned[w]) frontier.push_back(w);
            if(shared_chain) {
                auto original=frontier;
                for(int v:original) for(int w:users[v]) if(!seen[w]&&!banned[w]) frontier.push_back(w);
                for(int w=initial;w<n;w++) if(target[w]&&!seen[w]) frontier.push_back(w);
            }
            std::sort(frontier.begin(),frontier.end());
            frontier.erase(std::unique(frontier.begin(),frontier.end()),frontier.end());
            IdList candidates;
            for(int w:frontier) for(auto& u:via[w]) {
                if(!seen[u.other]) continue;
                int h=u.helper;
                if(stamp[h]!=attempt) {
                    stamp[h]=attempt; weight[h]=0; last_use[h]=-1;
                    if(shared_chain) {candidates.push_back(h);weight[h]=1;}
                    else for(auto e:makers[h]) if(seen[e.a]&&seen[e.b]) {candidates.push_back(h);weight[h]=1;break;}
                }
                if(weight[h] && last_use[h]!=w) {weight[h]++;last_use[h]=w;}
            }
            auto priority=[&](int a,int b) {
                if(weight[a]!=weight[b]) return weight[a]>weight[b];
                if(uses_mid[a].size()!=uses_mid[b].size()) return uses_mid[a].size()>uses_mid[b].size();
                return a<b;
            };
            int count=std::min<int>(max_neighbors,candidates.size());
            std::partial_sort(candidates.begin(),candidates.begin()+count,candidates.end(),priority);
            for(int ci=0;ci<count;ci++) {
                int h=candidates[ci];
                auto trial_seen=seen;
                auto trial_pick=chosen;
                propagate(trial_seen,trial_pick,h);
                if(satisfied(trial_seen)) {
                    bool simple=false;
                    for(auto e:makers[h]) if(seen[e.a]&&seen[e.b]) {
                        simple=true;
                        if(accept(trial_seen,trial_pick,h,e)) return true;
                    }
                    if(!simple && shared_chain) {
                        Graph trial;
                        for(int i=0;i<n;i++) if(trial_seen[i]) trial.emplace(values[i],trial_pick[i]);
                        Value value=helpers[h];trial[value]={0,0,0};
                        std::vector<Value> needed;
                        if(!order_for(trial,targets,needed) || needed.size()+2>=best.size()) continue;
                        for(int f=0;f<n;f++) if(seen[f]) for(auto end:inverses(value,values[f])) {
                            Value aux=end.a==values[f]?end.b:end.a;
                            auto m=mid_index.find(aux);
                            if(aux==value || m==mid_index.end()) continue;
                            for(auto e:makers[m->second]) if(seen[e.a]&&seen[e.b]) {
                                trial[aux]={e.op,values[e.a],values[e.b]};
                                trial[value]=end;
                                std::vector<Value> order;
                                Tape candidate;
                                if(order_for(trial,targets,order)&&order.size()<best.size()&&compile(trial,candidate)) {
                                    graph=live_only(trial,order);best=std::move(candidate);return true;
                                }
                                trial.erase(aux);
                            }
                        }
                    }
                } else if(two_helpers && holes.size()>=3 && ci<8) {
                    std::map<int,int> second_candidates;
                    for(int w:frontier) if(!trial_seen[w]) for(const auto& u:via[w]) {
                        if(trial_seen[u.other] && u.helper!=h) ++second_candidates[u.helper];
                    }
                    std::vector<std::pair<int,int>> ranked;
                    for(auto [other,score]:second_candidates)
                        ranked.push_back({score*1000+int(uses_mid[other].size()),other});
                    std::sort(ranked.rbegin(),ranked.rend());
                    int examined=0;
                    for(auto [score,other]:ranked) {
                        auto m=std::find_if(makers[other].begin(),makers[other].end(),[&](Edge e){return trial_seen[e.a]&&trial_seen[e.b];});
                        if(m==makers[other].end()) continue;
                        auto new_seen=trial_seen;
                        auto new_pick=trial_pick;
                        propagate(new_seen,new_pick,h,other);
                        if(satisfied(new_seen)) for(auto e:makers[h]) if(seen[e.a]&&seen[e.b]&&accept(new_seen,new_pick,h,e,other,*m)) return true;
                        if(++examined>=12) break;
                    }
                }
            }
        }
        return false;
    }

    struct PairRule {int a,b;char op;};
    struct Use {int output, other; Recipe step;};
    struct Middle {Value value; std::vector<PairRule> make; std::vector<Use> use;};
    using Bits=std::array<uint64_t,8>;

    bool knockout(Graph& graph,Tape& best, std::vector<std::pair<Value,Value>>* swaps=nullptr, std::vector<Graph>* neighbors=nullptr, int width=0) {
        std::vector<Value> order;
        if(!order_for(graph,targets,order)) return false;
        graph=live_only(graph,order);
        std::vector<Value> val=starts;
        std::sort(val.begin(),val.end());
        val.erase(std::unique(val.begin(),val.end()),val.end());
        int first=int(val.size());
        val.insert(val.end(),order.begin(),order.end());
        int n=int(val.size()),words=(n+63)/64;
        if(words>8) return false;
        std::unordered_map<Value,int> id;
        for(int i=0;i<n;i++) id[val[i]]=i;
        std::vector<Middle> lib;
        std::vector<std::vector<PairRule>> direct(n);
        std::vector<int> roots;
        std::vector<bool> required(n);
        for(Value t:targets){int i=id.at(t);if(i>=first)roots.push_back(i);required[i]=true;}
        for(int i=first;i<n;i++){auto r=graph.at(val[i]);direct[i].push_back({id.at(r.a),id.at(r.b),r.op});}
        // Store one witness inline until a different pair can produce the same integer.
        // Reversing each arithmetic identity gives its consumers without a second all-pairs scan.
        struct FirstWitness { Value key = 0; PairRule edge{}; int active = -1; };
        size_t capacity = 16;
        while (capacity < size_t(n) * (n + 1) * 3) capacity *= 2;
        std::vector<FirstWitness> table(capacity);
        std::unordered_set<Value> unary;
        for (int i=first;i<n;++i) {
            Value x=val[i];
            if (x%2==0) unary.insert(x/2);
            Value root=static_cast<Value>(std::sqrt(static_cast<long double>(x)));
            while (root+1<=x/(root+1)) ++root;
            while (root>x/root) --root;
            if (x%root==0 && x/root==root) unary.insert(root);
        }
        auto append_identity = [&](Middle& m, PairRule e) {
            m.make.push_back(e);
            int a=e.a,b=e.b;
            Value x=m.value;
            auto use=[&](int output,int other,Recipe r) {
                if(output>=first && output!=other) m.use.push_back({output,other,r});
            };
            switch(e.op) {
            case '+': use(a,b,{'-',x,val[b]}); use(b,a,{'-',x,val[a]}); break;
            case '-': use(a,b,{'+',x,val[b]}); use(b,a,{'-',val[a],x}); break;
            case '*': use(a,b,{'/',x,val[b]}); use(b,a,{'/',x,val[a]}); break;
            case '/': use(a,b,{'*',x,val[b]}); use(b,a,{'/',val[a],x}); break;
            }
        };
        for (int i=0;i<n;++i) for (int j=0;j<=i;++j) {
            int a=val[i]>=val[j]?i:j,b=i+j-a;
            for(char op:{'+','-','*','/'}) {
                Value x=calculate(op,val[a],val[b]);
                if (!x || x==val[a] || x==val[b]) continue;
                auto known=id.find(x);
                if(known!=id.end()){
                    if(known->second>=first)direct[known->second].push_back({a,b,op});
                    continue;
                }
                uint64_t h=static_cast<uint64_t>(x);
                h^=h>>30; h*=0xbf58476d1ce4e5b9ULL; h^=h>>27;
                size_t pos=(h*0x94d049bb133111ebULL >> 17)&(capacity-1);
                while(table[pos].key && table[pos].key!=x)pos=(pos+1)&(capacity-1);
                auto& s=table[pos];
                if(!s.key){
                    s.key=x;s.edge={a,b,op};
                    if(!unary.count(x))continue;
                    s.active=int(lib.size());lib.push_back({x,{},{}});
                } else if(s.active<0) {
                    if(s.edge.a==a && s.edge.b==b)continue;
                    s.active=int(lib.size());lib.push_back({x,{},{}});
                    append_identity(lib.back(),s.edge);
                }
                append_identity(lib[s.active],{a,b,op});
            }
        }
        for(auto& h:lib) for(char op:{'+','*','/'}) {
            auto it=id.find(calculate(op,h.value,h.value));
            if(it!=id.end() && it->second>=first)h.use.push_back({it->second,-1,{op,h.value,h.value}});
        }

        std::vector<Bits> allow(n),base(n);
        Bits selectable{};
        for(int i=first;i<n;i++)if(!required[i])selectable[i/64]|=uint64_t(1)<<(i%64);
        for(int i=0;i<n;i++) {
            allow[i]=selectable;allow[i][i/64]&=~(uint64_t(1)<<(i%64));
            if(i<first)base[i]=selectable;
        }
        auto close_word=[&](std::vector<Bits>& rows,int w,const Middle* h,uint64_t hmask){
            bool changed=true;
            while(changed){
                changed=false;
                for(int v=first;v<n;v++){
                    uint64_t previous=rows[v][w],open=allow[v][w]&~previous;
                    if(!open)continue;
                    uint64_t gain=0;
                    for(auto e:direct[v])gain|=rows[e.a][w]&rows[e.b][w];
                    gain&=open;
                    if(gain){rows[v][w]|=gain;changed=true;}
                }
                if(h)for(auto &u:h->use){
                    uint64_t gain=hmask&allow[u.output][w]&~rows[u.output][w];
                    if(u.other>=0)gain&=rows[u.other][w];
                    if(gain){rows[u.output][w]|=gain;changed=true;}
                }
            }
        };
        auto reconstruct=[&](const Middle* h,int ban1,int ban2,Graph* neutral=nullptr) {
            std::vector<unsigned char> got(n);
            std::fill_n(got.begin(),first,1);
            std::vector<Recipe> chosen(n);
            Recipe helper{};
            bool progress=true;
            while(progress){
                progress=false;
                for(int v=first;v<n;v++)if(v!=ban1&&v!=ban2&&!got[v])for(auto e:direct[v]){
                    if(got[e.a]&&got[e.b]){got[v]=1;chosen[v]={e.op,val[e.a],val[e.b]};progress=true;break;}
                }
                if(h&&!helper.op)for(auto e:h->make)if(got[e.a]&&got[e.b]){
                    helper={e.op,val[e.a],val[e.b]};progress=true;break;
                }
                if(h&&helper.op)for(auto &u:h->use)if(!got[u.output]&&u.output!=ban1&&u.output!=ban2&&(u.other<0||got[u.other])){
                    got[u.output]=1;chosen[u.output]=u.step;progress=true;
                }
            }
            for(int v:roots)if(!got[v])return false;
            Graph trial;
            for(int i=0;i<n;i++)if(got[i])trial.emplace(val[i],chosen[i]);
            if(h&&helper.op)trial.emplace(h->value,helper);
            std::vector<Value> live;
            Tape tape;
            if(!order_for(trial,targets,live))return false;
            if(neutral) {
                if(live.size()!=best.size() || !helper.op || !h) return false;
                *neutral=live_only(trial,live);
                return neutral->count(h->value)>0;
            }
            if(live.size()>=best.size()||!compile(trial,tape))return false;
            graph=live_only(trial,live);best=std::move(tape);return true;
        };
        Bits base_ok{};
        for(int w=0;w<words;w++){
            close_word(base,w,nullptr,0);
            base_ok[w]=selectable[w];for(int t:roots)base_ok[w]&=base[t][w];
        }
        for(int i=first;i<n;i++)if((base_ok[i/64]>>(i%64))&1)if(reconstruct(nullptr,i,-1))return true;
        #ifdef TWO_STEP_BRIDGE
        #include "two_step.inc"
        #endif
        std::shuffle(lib.begin(),lib.end(),rng);
        std::map<int,std::vector<int>> exchange_for;
        for(auto& h:lib) {
            if(h.use.empty())continue;
            Bits viable=base_ok;
            std::vector<Bits> rows;
            for(int w=0;w<words;w++){
                uint64_t m=0;
                for(auto e:h.make)m|=base[e.a][w]&base[e.b][w];
                if(!m)continue;
                bool useful=false;
                for(auto &u:h.use){
                    uint64_t delta=m&allow[u.output][w]&~base[u.output][w];
                    if(u.other>=0)delta&=base[u.other][w];
                    if(delta){useful=true;break;}
                }
                if(!useful)continue;
                if(rows.empty())rows=base;
                close_word(rows,w,&h,m);
                uint64_t ok=selectable[w];
                for(int v:roots)ok&=rows[v][w];
                viable[w]=ok;
            }
            if(rows.empty())continue;
            std::vector<int> drops;
            for(int w=0;w<words;w++){
                uint64_t bits=viable[w];
                while(bits){drops.push_back(64*w+__builtin_ctzll(bits));bits&=bits-1;}
            }
            if(neighbors) for(int x:drops) if(!((base_ok[x/64]>>(x%64))&1))
                exchange_for[x].push_back(int(&h-lib.data()));
            if(swaps) for(int x:drops) if(!((base_ok[x/64]>>(x%64))&1))
                swaps->push_back({val[x],h.value});
            if(drops.size()<2)continue;
            for(size_t i=0;i<drops.size();i++){
                if(reconstruct(&h,drops[i],-1))return true;
                for(size_t j=0;j<i;j++)if(reconstruct(&h,drops[i],drops[j]))return true;
            }
        }
        if(neighbors && width>0) {
            std::vector<int> pivots;
            for(auto& [pivot,choices]:exchange_for) {
                pivots.push_back(pivot);std::shuffle(choices.begin(),choices.end(),rng);
            }
            bool more=true;
            while(more && int(neighbors->size())<width){
                more=false;
                std::shuffle(pivots.begin(),pivots.end(),rng);
                for(int pivot:pivots) {
                    auto& choices=exchange_for[pivot];
                    if(choices.empty())continue;
                    more=true;
                    int h=choices.back();choices.pop_back();Graph result;
                    if(reconstruct(&lib[h],pivot,-1,&result))neighbors->push_back(std::move(result));
                    if(int(neighbors->size())==width)break;
                }
            }
        }
        return false;
    }

    bool crossover(const Graph& base, const std::vector<Graph>& archives,
                   Graph& winning, Tape& best, const std::vector<Value>& extras={}, const std::unordered_set<Value>& holes={}) {
        std::vector<Value> values=starts;
        std::sort(values.begin(),values.end());
        values.erase(std::unique(values.begin(),values.end()),values.end());
        int initial=int(values.size());
        std::unordered_map<Value,int> ids;
        for(int i=0;i<initial;i++)ids[values[i]]=i;
        auto append=[&](const Graph& g){
            std::vector<Value> seq;
            if(!order_for(g,targets,seq))return;
            for(Value x:seq)if(!ids.count(x)){ids[x]=int(values.size());values.push_back(x);}
        };
        append(base);
        const int orig=int(values.size());
        for(auto& g:archives)append(g);
        for(Value x:extras) if(!ids.count(x)){ids[x]=int(values.size());values.push_back(x);}
        int n=int(values.size());
        std::vector<std::vector<PairRule>> recipes(n);
        for(int i=initial;i<orig;i++){auto r=base.at(values[i]);recipes[i].push_back({ids.at(r.a),ids.at(r.b),r.op});}
        for(auto& g:archives)for(auto [v,r]:g)if(r.op&&ids.count(v)&&ids.count(r.a)&&ids.count(r.b))
            recipes[ids.at(v)].push_back({ids.at(r.a),ids.at(r.b),r.op});
        for(int i=0;i<n;i++)for(int j=0;j<=i;j++) {
            int a=values[i]>=values[j]?i:j,b=i+j-a;
            for(char op:{'+','-','*','/'}){
                Value x=calculate(op,values[a],values[b]);
                auto it=ids.find(x);
                if(it!=ids.end()&&it->second>=initial&&x!=values[a]&&x!=values[b])recipes[it->second].push_back({a,b,op});
            }
        }
        std::vector<PairRule> edges;
        std::vector<int> cuts(n+1),needed(n);
        int goal_count=0;
        for(auto v:targets)if(ids.at(v)>=initial){needed[ids.at(v)]=1;++goal_count;}
        for(int i=initial;i<n;i++){cuts[i]=int(edges.size());edges.insert(edges.end(),recipes[i].begin(),recipes[i].end());}
        cuts[n]=int(edges.size());
        std::vector<unsigned char> banned(n),seen(n);
        std::vector<int> pick(n);
        auto close=[&]() {
            int remaining=goal_count;
            std::fill(seen.begin(),seen.end(),0);
            std::fill_n(seen.begin(),initial,1);
            bool progress=true;
            while(progress&&remaining) {
                progress=false;
                for(int v=initial;v<n;v++)if(!seen[v]&&!banned[v]){
                    for(int j=cuts[v];j<cuts[v+1];j++){
                        auto r=edges[j];if(!seen[r.a]||!seen[r.b])continue;
                        seen[v]=1;pick[v]=j;remaining-=needed[v];progress=true;break;
                    }
                }
            }
            return !remaining;
        };
        std::vector<int> support(n);
        for(auto& g:archives)for(auto [v,r]:g)if(ids.count(v))++support[ids.at(v)];
        std::vector<int> optional;
        for(int i=initial;i<n;i++)if(!needed[i])optional.push_back(i);
        for(int attempt=0;attempt<7;attempt++){
            std::vector<std::pair<double,int>> ranking;
            for(int i:optional){
                double x=double(rng())/std::mt19937::max();
                if(attempt%3!=0)x+=(attempt%3==1?0.22:0.09)*support[i];
                if(!extras.empty() && attempt%3!=0) {
                    if(i<orig) x+=0.32;
                    if(holes.count(values[i])) x-=0.18+0.11*attempt;
                }
                ranking.emplace_back(x,i);
            }
            std::sort(ranking.begin(),ranking.end());
            std::fill(banned.begin(),banned.end(),0);
            for(auto [priority,i]:ranking){banned[i]=1;if(!close())banned[i]=0;}
            if(!close())continue;
            Graph trial;
            for(int i=0;i<initial;i++)trial[values[i]]={0,0,0};
            for(int i=initial;i<n;i++)if(seen[i]){auto r=edges[pick[i]];trial[values[i]]={r.op,values[r.a],values[r.b]};}
            std::vector<Value> sequence;
            Tape tape;
            if(order_for(trial,targets,sequence)&&sequence.size()<best.size()&&compile(trial,tape)){
                winning=live_only(trial,sequence);best=std::move(tape);return true;
            }
        }
        return false;
    }


    struct RecipeLookup {
        struct Cell { Value key=0; int head=-1, count=0; };
        struct Link { Recipe rule; int previous; };
        std::vector<Cell> slots;
        std::vector<Link> links;
        std::vector<size_t> occupied;
        void reset(size_t n) {
            size_t capacity=16;
            while(capacity<3*n*(n+1))capacity*=2;
            if(slots.size()<capacity){slots.assign(capacity,{});}
            else for(size_t i:occupied)slots[i]=Cell{};
            occupied.clear();links.clear();
        }
        size_t locate(Value x)const {
            uint64_t h=static_cast<uint64_t>(x);
            h^=h>>30;h*=0xbf58476d1ce4e5b9ULL;h^=h>>27;
            size_t i=(h*0x94d049bb133111ebULL>>17)&(slots.size()-1);
            while(slots[i].key&&slots[i].key!=x)i=(i+1)&(slots.size()-1);
            return i;
        }
        void push(Value x,Recipe r){
            size_t i=locate(x);auto& s=slots[i];
            if(!s.key){s.key=x;occupied.push_back(i);}
            links.push_back({r,s.head});s.head=int(links.size())-1;++s.count;
        }
        bool contains(Value x)const{return slots[locate(x)].key!=0;}
        bool sample(Value x,std::mt19937& rng,Recipe& result)const{
            const auto& s=slots[locate(x)];if(!s.count)return false;
            int j=s.head,skip=rng()%s.count;
            while(skip--)j=links[j].previous;
            result=links[j].rule;return true;
        }
        template<class Predicate>
        bool first(Value x,Predicate allowed,Recipe& result)const{
            for(int j=slots[locate(x)].head;j>=0;j=links[j].previous){
                auto r=links[j].rule;
                if(allowed(r)){result=r;return true;}
            }
            return false;
        }
        std::vector<Value> universe;
        std::unordered_set<Value> indexed;
        void prepare(const std::vector<Value>& active,Value limit){
            size_t additions=0;
            for(Value x:active)if(!indexed.count(x))++additions;
            if(universe.empty()||universe.size()+additions>active.size()*5/4+15){
                reset(active.size()*5/4+17);universe.clear();indexed.clear();
            }
            for(Value x:active)if(indexed.insert(x).second){
                universe.push_back(x);
                for(Value other:universe){
                    Value a=std::max(x,other),b=std::min(x,other);
                    if(a<=limit-b)push(a+b,{'+',a,b});
                    if(a>b && a-b!=b)push(a-b,{'-',a,b});
                    if(b>1 && a<=limit/b)push(a*b,{'*',a,b});
                    if(b>1 && a%b==0 && a/b!=b)push(a/b,{'/',a,b});
                }
            }
        }
        template<class Predicate>
        bool sample_active(Value x,std::mt19937& generator,Predicate live,Recipe& answer)const{
            const Cell& cell=slots[locate(x)];
            if(!cell.count)return false;
            int offset=generator()%cell.count;
            int j=cell.head;for(int i=0;i<offset;++i)j=links[j].previous;
            int current=j;
            for(int i=0;i<cell.count;++i){
                auto r=links[current].rule;
                if(live(r.a)&&live(r.b)){answer=r;return true;}
                current=links[current].previous;
                if(current<0)current=cell.head;
            }
            return false;
        }

    } lookup;

    // Explore equal-length programs with different intermediate values.
    bool perturb(Graph& graph, size_t cap, int wanted) {
        std::vector<Value> order;
        if (!order_for(graph, targets, order)) return false;
        graph = live_only(graph, order);
        std::vector<Value> numbers = starts;
        numbers.insert(numbers.end(), order.begin(), order.end());
        std::shuffle(numbers.begin(), numbers.end(), rng);
        lookup.prepare(numbers,limit);
        std::shuffle(order.begin(), order.end(), rng);
        std::unordered_map<Value,int> used;
        auto refresh = [&]() {
            used.clear();
            for (auto root : targets) ++used[root];
            for (auto [output, step] : graph) if (step.op) {
                ++used[step.a];
                if (step.a != step.b) ++used[step.b];
            }
        };
        refresh();
        int swaps = 0;
        for (Value v : order) {
            if (!graph.count(v)) continue;
            Recipe old = graph.at(v);
            bool has_private = (graph.at(old.a).op && used[old.a] == 1) ||
                               (graph.at(old.b).op && used[old.b] == 1);
            auto replace = [&](Recipe step, Value extra, Recipe build) {
                if (step.a == v || step.b == v) return false;
                if (extra) {
                    if (extra == v || graph.count(extra) || !graph.count(build.a) || !graph.count(build.b)) return false;
                    graph.emplace(extra, build);
                }
                graph[v] = step;
                std::vector<Value> live;
                bool valid = order_for(graph, targets, live) && live.size() <= cap;
                if (valid) graph = live_only(graph, live);
                else {
                    graph[v] = old;
                    if (extra) graph.erase(extra);
                }
                return valid;
            };
            bool changed = false;
            int attempts = 0;
            size_t shift = rng() % numbers.size();
            for (size_t j = 0; j < numbers.size() && !changed; ++j) {
                Value fixed = numbers[(shift+j) % numbers.size()];
                if (fixed == v || !graph.count(fixed)) continue;
                for (auto step : inverses(v, fixed)) {
                    Value need = step.a == fixed ? step.b : step.a;
                    if (need == v || (step.op == old.op &&
                        ((step.a == old.a && step.b == old.b) ||
                         ((step.op=='+' || step.op=='*') && step.a==old.b && step.b==old.a)))) continue;
                    if (graph.count(need)) {
                        if (rng()%4 == 0 && replace(step, 0, {})) {changed=true;break;}
                    } else if (has_private || cap > order.size()) {
                        Recipe r; if (!lookup.sample_active(need,rng,[&](Value x){return graph.count(x)>0;},r)) continue;
                        if (r.a==v || r.b==v) continue;
                        if (cap<=order.size()) {
                            bool loses_a = used[old.a]==1 && graph.at(old.a).op && r.a!=old.a && r.b!=old.a && fixed!=old.a;
                            bool loses_b = used[old.b]==1 && graph.at(old.b).op && r.a!=old.b && r.b!=old.b && fixed!=old.b;
                            if(!loses_a && !loses_b) continue;
                        }
                        if (replace(step, need, r)) {changed=true;break;}
                        if (++attempts >= 12) break;
                    }
                }
                if (attempts>=12) break;
            }
            if (changed) { if (++swaps >= wanted) break; refresh(); }
        }
        return swaps > 0;
    }


    bool rotate(Graph& g,size_t cap) {
        std::vector<Value> original;
        if(!order_for(g,targets,original))return false;
        g=live_only(g,original);
        std::unordered_map<Value,int> uses;
        for(Value t:targets)++uses[t];
        for(auto [v,r]:g)if(r.op){++uses[r.a];++uses[r.b];}
        std::vector<Value> vals=original;
        vals.insert(vals.end(),starts.begin(),starts.end());
        std::shuffle(vals.begin(),vals.end(),rng);
        lookup.prepare(vals,limit);
        std::vector<Value> roots=original;
        std::shuffle(roots.begin(),roots.end(),rng);
        int scanned=0;
        for(Value v:roots){
            std::unordered_set<Value> cut{v};
            std::vector<Value> changed;
            auto clear=[&](auto&& self,Value x)->void{
                changed.push_back(x);if(--uses[x]||!g.at(x).op)return;
                cut.insert(x);auto r=g.at(x);self(self,r.a);self(self,r.b);
            };
            auto old=g.at(v);
            clear(clear,old.a);clear(clear,old.b);
            for(Value x:changed)++uses[x];
            if(cut.size()<3)continue;
            std::unordered_set<Value> banned=cut,leaves;
            for(Value x:cut)for(Value a:{g.at(x).a,g.at(x).b})if(!cut.count(a))leaves.insert(a);
            for(Value x:original){auto r=g.at(x);if(banned.count(r.a)||banned.count(r.b))banned.insert(x);}
            std::vector<Value> fixed;
            for(Value x:vals)if(!banned.count(x))fixed.push_back(x);
            std::stable_partition(fixed.begin(),fixed.end(),[&](Value x){return leaves.count(x);});
            if(fixed.empty())continue;
            auto maker=[&](Value x,Recipe& answer){
                return !g.count(x) && lookup.first(x,[&](Recipe r){return g.count(r.a)&&g.count(r.b)&&!banned.count(r.a)&&!banned.count(r.b);},answer);
            };
            auto accept=[&](Value a,Recipe ra,Value b,Recipe rb,Recipe end){
                if(a==b||a==v||b==v)return false;
                g[a]=ra;g[b]=rb;g[v]=end;
                std::vector<Value> live;
                if(order_for(g,targets,live)&&live.size()<=cap){g=live_only(g,live);return true;}
                g.erase(a);g.erase(b);g[v]=old;return false;
            };
            auto balanced=[&]() {
                int tests=0;
                int width=std::min<int>(fixed.size(),24);
                std::vector<Value> temp;
                for(int i=0;i<width;i++)for(int j=0;j<=i;j++){
                    Value aa=std::max(fixed[i],fixed[j]),bb=std::min(fixed[i],fixed[j]);
                    for(char op:{'+','-','*','/'}) {
                        Value h=calculate(op,aa,bb);
                        if(h&&!g.count(h)&&lookup.contains(h))temp.push_back(h);
                    }
                }
                std::shuffle(temp.begin(),temp.end(),rng);
                for(Value a:temp)for(auto end:inverses(v,a)){
                    Value b=end.a==a?end.b:end.a;
                    Recipe ra,rb;
                    if(a!=b&&maker(b,rb)&&maker(a,ra)){
                        if(accept(a,ra,b,rb,end))return true;
                        if(++tests>=12)return false;
                    }
                }
                return false;
            };
            auto chain=[&](){
                int width=std::min<int>(fixed.size(),32),tested=0;
                for(int i=0;i<width;i++)for(auto end:inverses(v,fixed[i])){
                    Value b=end.a==fixed[i]?end.b:end.a;
                    if(g.count(b))continue;
                    for(int j=0;j<width;j++)for(auto rb:inverses(b,fixed[j])){
                        Value a=rb.a==fixed[j]?rb.b:rb.a;
                        Recipe ra;
                        if(a!=b&&maker(a,ra)){
                            if(accept(a,ra,b,rb,end))return true;
                            if(++tested>=12)return false;
                        }
                    }
                }
                return false;
            };
            if((rng()%2)?(balanced()||chain()):(chain()||balanced()))return true;
            if(++scanned>=12)break;
        }
        return false;
    }
    Tape improve(const std::vector<Tape>& tapes){
        Tape best=tapes.front();Graph champion=graph_from(best),trial=champion;
        std::vector<Graph> pool;
        for(int turn=0;turn<rounds;++turn){
            if(turn%9==0)trial=champion;
            else if(turn%9==5&&!pool.empty())trial=pool[rng()%pool.size()];
            std::shuffle(targets.begin(),targets.end(),rng);
            size_t cap=best.size()+((turn%9==1||turn%9==2)?2:turn%9<=5?1:0);
            perturb(trial,cap,3+rng()%8);
            rotate(trial,cap);
            Tape candidate;
            if(!compile(trial,candidate)||candidate.size()>cap){trial=champion;continue;}
            if(candidate.size()<best.size()){best=candidate;champion=trial;}
            if(knockout(trial,candidate)&&candidate.size()<best.size()){best=candidate;champion=trial;}
            if(candidate.size()<=best.size()+1){
                pool.push_back(trial);
                if(pool.size()>12)pool.erase(pool.begin()+rng()%5);
            }
            if(turn%5==4&&pool.size()>=4){
                Graph better;
                if(crossover(champion,pool,better,best)){champion=better;trial=better;}
            }
        }
        return best;
    }

};
int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    int count;
    if (!(std::cin >> count)) return 1;
    for (int puzzle_index = 0; puzzle_index < count; ++puzzle_index) {
        Puzzle puzzle;
        int size, seed_count;
        unsigned seed;
        std::cin >> puzzle.limit >> puzzle.registers >> puzzle.rounds >> puzzle.max_neighbors >> seed;
        puzzle.rng.seed(seed);
        std::cin >> size;
        puzzle.starts.resize(size);
        for (auto& value : puzzle.starts) std::cin >> value;
        std::cin >> size;
        puzzle.targets.resize(size);
        for (auto& value : puzzle.targets) std::cin >> value;
        std::cin >> seed_count;
        std::vector<Tape> tapes(seed_count);
        for (auto& tape : tapes) {
            std::cin >> size;
            tape.resize(size);
            for (auto& [op, a, b, dst] : tape) std::cin >> op >> a >> b >> dst;
        }
        if (!std::cin || tapes.empty()) return 2;
        puzzle.remember_order(tapes.front());
        auto best = puzzle.improve(tapes);
        std::cout << best.size();
        for (auto [op, a, b, dst] : best) std::cout << ' ' << op << ' ' << a << ' ' << b << ' ' << dst;
        std::cout << '\n';
    }
}

