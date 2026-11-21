#define ELF_INTERFACE_RELAY_LIBRARY
#include "elf_interface_relay.cpp"

int main(){
    constexpr Value bound=1400;
    Graph g;
    g.initial.fill(-1);for(int i=0;i<3;++i)g.initial[i]=i;
    g.nodes={{3,0,-1,-1},{4,0,-1,-1},{5,0,-1,-1},{2,'-',2,0},{7,'+',2,3},
             {10,'+',4,0},{100,'*',5,5},{1000,'*',5,6}};
    g.targets={6,7};
    const auto c=make_cut(g,g.targets);
    relay_interface::Completion calc(g,c);
    int retired=0;
    const auto old=calc.close({},retired);
    if(old.size()!=5||retired)return 1;
    const auto p=calc.close({{10,'+',5,5}},retired);
    if(p.size()!=3||retired!=2||p.back().x!=1000)return 2;
    const auto out=replace_block(g,c,p,bound,5,12);
    if(out.size()!=3)return 3;
    Registers r{};r[0]=3;r[1]=4;r[2]=5;
    std::unordered_set<Value> seen;
    for(auto z:out)seen.insert(execute(r,z,bound));
    if(!seen.count(100)||!seen.count(1000))return 4;
    // Invalid cyclic nominations fail, not silently become pre-paid values.
    if(!calc.close({{10,'/',100,10}},retired).empty())return 5;
    std::cout<<"Inside-only replacement preserves both old consumers, retires two helpers and passes physical replay\n";
}
