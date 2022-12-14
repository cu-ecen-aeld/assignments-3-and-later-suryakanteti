The following is the message printed upon encountering a kernel oops in the faulty driver module with the numerically quoted bold lines giving us more information about the error:

1. A null pointer dereference has caused the kernel oops
2. The error has occurred 0x14 bytes (20 bytes) into the function faulty_write which has a total length of 0x20 bytes (32 bytes)

<pre>
<b>1. Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000</b>

Mem abort info:

  ESR = 0x96000045
  
  EC = 0x25: DABT (current EL), IL = 32 bits
  
  SET = 0, FnV = 0
  
  EA = 0, S1PTW = 0
  
  FSC = 0x05: level 1 translation fault
  
Data abort info:

  ISV = 0, ISS = 0x00000045
  
  CM = 0, WnR = 1
  
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042627000

[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000

Internal error: Oops: 96000045 [#1] SMP

Modules linked in: hello(O) faulty(O) scull(O)

CPU: 0 PID: 159 Comm: sh Tainted: G           O      5.15.18 #1

Hardware name: linux,dummy-virt (DT)

pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)

pc : faulty_write+0x14/0x20 [faulty]

lr : vfs_write+0xa8/0x2a0

sp : ffffffc008d23d80

x29: ffffffc008d23d80 x28: ffffff80026a9980 x27: 0000000000000000

x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000

x23: 0000000040000000 x22: 0000000000000012 x21: 000000558b449a00

x20: 000000558b449a00 x19: ffffff80026e1200 x18: 0000000000000000

x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000

x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000

x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000

x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000

x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d23df0

x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000

Call trace:

<b>2. faulty_write+0x14/0x20 [faulty]</b>
 
 ksys_write+0x68/0x100
 
 __arm64_sys_write+0x20/0x30
 
 invoke_syscall+0x54/0x130
 
 el0_svc_common.constprop.0+0x44/0x100
 
 do_el0_svc+0x44/0xb0
 
 el0_svc+0x28/0x80
 
 el0t_64_sync_handler+0xa4/0x130
 
 el0t_64_sync+0x1a0/0x1a4
 
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 

---[ end trace 729738912a160441 ]---


</pre>
