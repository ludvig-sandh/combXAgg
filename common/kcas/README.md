# PathCAS synchronization library

PathCAS is a new synchronization mechanism for concurrent data structures introduced in a PPoPP'22 paper.

PathCAS: An Efficient Middle Ground for Concurrent Search Data Structures
https://dl.acm.org/doi/pdf/10.1145/3503221.3508410

The paper has a link to the code artifact for running the experiments in the paper.

For the implementation of PathCAS itself, see: kcas_validate.h

A version that is optimized using Intel's restricted transactional memory can be found in: kcas_validate_htm.h

You can find a repo containing several data structures that use PathCAS here: https://gitlab.com/trbot86/tmbench
Specifically, see ds/*_kcas_validate.

A good example data structure to mimic if you'd like to use PathCAS is: https://gitlab.com/trbot86/tmbench/-/blob/master/ds/sigouin_int_bst_kcas_validate_htm/internal_kcas.h

Some slides that motivate and briefly explain how to use PathCAS (from a talk Trevor gave at the University of Windsor): https://cs.uwaterloo.ca/~t35brown/files/ppopp22_pathcas_windsor_final.pptx
