<?php
class problemController extends Controller 
{

    public function __construct() 
    {
        parent::__construct(__CLASS__);
    }
    
    
    
    public function index($arr)
    {
        $problem_id = $arr['problem_id'];
        
        $data['problem_id'] = $problem_id;
        $data['page_title'] = 'Problem'.$problem_id;
        $data['title'] = $this->load('standard')->encodehtml($this->M->getTitle($problem_id));
        $data['time_limit'] = $this->M->getTimeLimit($problem_id);
        $data['memory_limit'] = $this->M->getMemoryLimit($problem_id);
        $data['description'] =  $this->load('standard')->encodehtml($this->M->getDescription($problem_id));
        $data['input'] = $this->load('standard')->encodehtml($this->M->getInput($problem_id));
        $data['output'] = $this->load('standard')->encodehtml($this->M->getOutput($problem_id));
        $data['sample_input'] = $this->load('standard')->encodehtml($this->M->getSampleInput($problem_id));
        $data['sample_output'] = $this->load('standard')->encodehtml($this->M->getSampleOutput($problem_id));
        $data['hint'] = $this->load('standard')->encodehtml($this->M->getHint($problem_id));
        $data['source'] = $this->load('standard')->encodehtml($this->M->getSource($problem_id));
        $this->showTemplate('problem',$data);
    }
    
}
?>
