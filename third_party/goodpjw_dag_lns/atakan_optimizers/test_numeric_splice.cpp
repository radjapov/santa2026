#define ELF_INTERFACE_CLOSURE_LIBRARY
#include "elf_interface_closure.cpp"

int main(){
    constexpr Value bound=1300;
    Graph g;
    g.initial.fill(-1);for(int i=0;i<4;++i)g.initial[i]=i;
    g.nodes={{3,0,-1,-1},{4,0,-1,-1},{5,0,-1,-1},{6,0,-1,-1},
             {12,'*',0,1},{18,'+',4,3},{24,'+',5,3},{576,'*',6,6}};
    g.targets={5,7};
    auto cut=make_cut(g,{5});
    if(cut.budget!=2||!cut.fixed[6]||cut.terminal[6])return 1;
    Script patch{{24,'*',4,6},{18,'-',24,6}};
    const auto pref=replace_block(g,cut,patch,bound,4,12);
    if(!pref.empty())return 2;
    const auto output=numeric_splice::apply(g,patch,bound,4,12);
    if(output.size()!=3)return 3;
    Registers r{};r[0]=3;r[1]=4;r[2]=5;r[3]=6;
    std::unordered_set<Value> seen;
    for(auto z:output)seen.insert(execute(r,z,bound));
    if(!seen.count(18)||!seen.count(576))return 4;
    if(!numeric_splice::apply(g,{{18,'-',24,6}},bound,4,12).empty())return 5;
    if(!numeric_splice::apply(g,{{24,'*',3,6}},bound,4,12).empty())return 6;
    std::cout<<"Neutral local rewrite removes a global duplicate; 4 to 3 physical writes; cycles rejected\n";
}
