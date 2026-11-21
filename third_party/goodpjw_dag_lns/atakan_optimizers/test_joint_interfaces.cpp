#define ELF_JOINT_INTERFACES_LIBRARY
#include "elf_joint_interfaces.cpp"

int main(){
    constexpr Value bound=800;
    Graph g;
    g.initial.fill(-1);for(int i=0;i<4;++i)g.initial[i]=i;
    g.nodes={{3,0,-1,-1},{4,0,-1,-1},{5,0,-1,-1},{6,0,-1,-1},{9,'+',3,0},
             {27,'*',4,0},{18,'-',5,4},{45,'*',4,2},{36,'-',7,4}};
    g.targets={6,8};
    const auto a=make_cut(g,{5,6}),b=make_cut(g,{7,8});
    if(!a.fixed[4]||!b.fixed[4])return 1; // Neither cut can retire 9 alone.
    combine_interfaces::Context ctx(g);
    const auto lp=ctx.literal_ports(a,b),rp=ctx.real_ports(a,b);
    if(lp!=std::vector<int>{5,6,7,8}||rp!=std::vector<int>{6,8})return 2;
    const auto literal=make_cut(g,lp),adjusted=make_cut(g,rp);
    if(adjusted.fixed[4]||literal.budget!=5||adjusted.budget!=5)return 3;
    combine_interfaces::Meter first,second;
    auto program=emit_graph(g,bound,20,16),original=program;
    if(program.size()!=5)return 4;
    combine_interfaces::try_cut(g,literal,bound,12,0,123,combine_interfaces::Deadline::max(),first,program);
    combine_interfaces::try_cut(g,adjusted,bound,12,1,123,combine_interfaces::Deadline::max(),second,original);
    if(program.size()!=5||original.size()!=2)return 5;
    g.targets.push_back(5);
    combine_interfaces::Context extra(g);
    if(extra.real_ports(a,b)!=std::vector<int>{5,6,8})return 6;
    std::cout<<"Coupled cut retires a shared ancestor; external scoring ports are preserved; 5 to 2 writes\n";
}
