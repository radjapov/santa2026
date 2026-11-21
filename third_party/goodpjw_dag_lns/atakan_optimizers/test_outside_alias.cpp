#define ELF_OUTSIDE_ALIAS_LIBRARY
#include "elf_outside_alias.cpp"

int main(){
    constexpr Value limit=4000;
    Graph g;
    g.initial.fill(-1);for(int i=0;i<4;++i)g.initial[i]=i;
    g.nodes={{3,0,-1,-1},{4,0,-1,-1},{5,0,-1,-1},{6,0,-1,-1},
             {16,'*',1,1},{32,'+',4,4},{36,'+',5,1},{1296,'*',6,6}};
    g.targets={5,7};
    const Cut c=make_cut(g,{5});
    auto best=emit_graph(g,limit,10,12),alt=best;
    outside_alias::Stats plain,mixed;
    const auto until=outside_alias::Deadline::max();
    outside_alias::apply(g,c,limit,12,0,19,until,best,plain);
    outside_alias::apply(g,c,limit,12,1,19,until,alt,mixed);
    if(best.size()!=4||alt.size()!=3||!mixed.joined)return 1;
    Registers r{};r[0]=3;r[1]=4;r[2]=5;r[3]=6;
    std::unordered_set<Value> values;
    for(auto z:alt)values.insert(execute(r,z,limit));
    if(!values.count(32)||!values.count(1296))return 2;
    std::cout<<"Paying an outside value first permits a joint 4 to 3 write rebuild\n";
}
